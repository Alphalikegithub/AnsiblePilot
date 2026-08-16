#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextStream>
#include <QTimer>
#include <QXmlStreamReader>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , process(new QProcess(this))
    , externalLogTimer(new QTimer(this))
{
    ui->setupUi(this);
    setupUiStyles();

    // 初始化运行历史表格
    ui->historyTable->setColumnCount(6);
    ui->historyTable->setHorizontalHeaderLabels({
        QStringLiteral("时间"),
        QStringLiteral("指令内容"),
        QStringLiteral("执行状态"),
        QStringLiteral("退出代码"),
        QStringLiteral("耗时"),
        QStringLiteral("操作")
    });
    ui->historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->historyTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    cleanupOldTempFiles();

    // 默认工作空间
    ui->workDirEdit->setText(QDir::currentPath());
    scanFiles();
    loadClusterConfig();
    updateInventoryGroups();
    updatePreview();

    // 定时轮询外部终端输出
    connect(externalLogTimer, &QTimer::timeout, this, &MainWindow::pollExternalTerminalOutput);
    externalLogTimer->start(350);

    // 内置进程信号
    connect(process, &QProcess::readyReadStandardOutput, this, [this]() {
        appendLog(QString::fromUtf8(process->readAllStandardOutput()));
    });
    connect(process, &QProcess::readyReadStandardError, this, [this]() {
        appendLog(QString::fromUtf8(process->readAllStandardError()));
    });

    // ================= 信号槽绑定 =================

    // 1. 基础配置
    connect(ui->selectWorkDirBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择工作目录"), workspace());
        if (!dir.isEmpty()) {
            ui->workDirEdit->setText(dir);
            scanFiles();
            updateInventoryGroups();
            updatePreview();
        }
    });
    connect(ui->selectInventoryBtn, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("选择 Inventory 文件"), workspace());
        if (!file.isEmpty()) {
            if (ui->inventoryCombo->findText(file) < 0) {
                ui->inventoryCombo->addItem(file);
            }
            ui->inventoryCombo->setCurrentText(file);
            updateInventoryGroups();
            updatePreview();
        }
    });
    connect(ui->previewInventoryBtn, &QPushButton::clicked, this, &MainWindow::openInventoryPreviewTab);
    connect(ui->reloadInventoryBtn, &QPushButton::clicked, this, &MainWindow::loadInventoryPreview);
    connect(ui->saveInventoryBtn, &QPushButton::clicked, this, &MainWindow::saveInventoryPreview);
    connect(ui->copyInventoryBtn, &QPushButton::clicked, this, &MainWindow::copyInventoryPreview);

    connect(ui->selectPlaybookBtn, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("选择 Playbook 剧本文件"), workspace(), QStringLiteral("Ansible 剧本 (*.yaml *.yml);;所有文件 (*.*)"));
        if (!file.isEmpty()) {
            if (ui->playbookCombo->findText(file) < 0) {
                ui->playbookCombo->addItem(file);
            }
            ui->playbookCombo->setCurrentText(file);
            updatePreview();
        }
    });
    connect(ui->previewPlaybookBtn, &QPushButton::clicked, this, &MainWindow::openPlaybookPreviewTab);
    connect(ui->reloadPlaybookBtn, &QPushButton::clicked, this, &MainWindow::loadPlaybookPreview);
    connect(ui->savePlaybookBtn, &QPushButton::clicked, this, &MainWindow::savePlaybookPreview);
    connect(ui->copyPlaybookBtn, &QPushButton::clicked, this, &MainWindow::copyPlaybookPreview);

    connect(ui->logTabWidget, &QTabWidget::currentChanged, this, [this](int idx) {
        if (ui->logTabWidget->widget(idx) == ui->tabInventoryPreview) {
            loadInventoryPreview();
        } else if (ui->logTabWidget->widget(idx) == ui->tabPlaybookPreview) {
            loadPlaybookPreview();
        }
    });
    connect(ui->refreshScanBtn, &QPushButton::clicked, this, [this]() {
        scanFiles();
        updateInventoryGroups();
        updatePreview();
    });
    connect(ui->workDirEdit, &QLineEdit::textChanged, this, [this]() {
        scanFiles();
        updateInventoryGroups();
        updatePreview();
    });
    connect(ui->inventoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onInventorySelected);
    connect(ui->hostGroupCombo, &QComboBox::currentTextChanged, this, &MainWindow::updatePreview);
    connect(ui->limitEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->extraArgsEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);

    // 选项卡联动
    connect(ui->mainTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onMainTabChanged);
    connect(ui->opsSubTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onOpsSubTabChanged);

    // 2. 常用操作 - 服务管理
    connect(ui->serviceNameEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->startServiceBtn, &QPushButton::clicked, this, [this]() {
        const QString s = ui->serviceNameEdit->text().trimmed();
        if (s.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入服务名。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("systemd"), QStringLiteral("name=%1 state=started").arg(s)), workspace());
    });
    connect(ui->stopServiceBtn, &QPushButton::clicked, this, [this]() {
        const QString s = ui->serviceNameEdit->text().trimmed();
        if (s.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入服务名。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("systemd"), QStringLiteral("name=%1 state=stopped").arg(s)), workspace());
    });
    connect(ui->restartServiceBtn, &QPushButton::clicked, this, [this]() {
        const QString s = ui->serviceNameEdit->text().trimmed();
        if (s.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入服务名。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("systemd"), QStringLiteral("name=%1 state=restarted").arg(s)), workspace());
    });
    connect(ui->statusServiceBtn, &QPushButton::clicked, this, [this]() {
        const QString s = ui->serviceNameEdit->text().trimmed();
        if (s.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入服务名。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("shell"), QStringLiteral("systemctl status %1 --no-pager").arg(s)), workspace());
    });

    // 2. 常用操作 - 系统信息
    connect(ui->diskInfoBtn, &QPushButton::clicked, this, [this]() {
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("command"), QStringLiteral("df -h")), workspace());
    });
    connect(ui->memInfoBtn, &QPushButton::clicked, this, [this]() {
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("command"), QStringLiteral("free -h")), workspace());
    });
    connect(ui->uptimeBtn, &QPushButton::clicked, this, [this]() {
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("command"), QStringLiteral("uptime")), workspace());
    });
    connect(ui->hostInfoBtn, &QPushButton::clicked, this, [this]() {
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("shell"), QStringLiteral("hostname && uname -a")), workspace());
    });
    connect(ui->cpuLoadBtn, &QPushButton::clicked, this, [this]() {
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("shell"), QStringLiteral("top -b -n1 | head -20")), workspace());
    });
    connect(ui->whoBtn, &QPushButton::clicked, this, [this]() {
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("command"), QStringLiteral("who")), workspace());
    });

    // 2. 常用操作 - 进程与日志
    connect(ui->presetProcCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPresetProcChanged);
    connect(ui->applyPresetProcBtn, &QPushButton::clicked, this, &MainWindow::applyPresetProc);
    connect(ui->procKeywordEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->logPathEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->logLinesEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->grepKeywordEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);

    connect(ui->psBtn, &QPushButton::clicked, this, [this]() {
        const QString kw = ui->procKeywordEdit->text().trimmed();
        const QString cmd = kw.isEmpty() ? QStringLiteral("ps -ef | head -50") : QStringLiteral("ps -ef | grep -v grep | grep %1").arg(kw);
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("shell"), cmd), workspace());
    });
    connect(ui->pkillBtn, &QPushButton::clicked, this, [this]() {
        const QString kw = ui->procKeywordEdit->text().trimmed();
        if (kw.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入要终止的进程关键字。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("shell"), QStringLiteral("pkill -f %1").arg(kw)), workspace());
    });
    connect(ui->tailLogBtn, &QPushButton::clicked, this, [this]() {
        const QString p = ui->logPathEdit->text().trimmed();
        const QString n = ui->logLinesEdit->text().trimmed().isEmpty() ? QStringLiteral("200") : ui->logLinesEdit->text().trimmed();
        if (p.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入日志文件路径。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("shell"), QStringLiteral("tail -n %1 %2").arg(n, p)), workspace());
    });
    connect(ui->grepLogBtn, &QPushButton::clicked, this, [this]() {
        const QString p = ui->logPathEdit->text().trimmed();
        const QString kw = ui->grepKeywordEdit->text().trimmed();
        if (p.isEmpty() || kw.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请完整填写日志路径和搜索关键词。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("shell"), QStringLiteral("grep -n -- %1 %2 | tail -n 200").arg(kw, p)), workspace());
    });

    // 2. 常用操作 - 文件操作
    connect(ui->fetchRemoteEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->fetchLocalEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->deleteRemoteEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->mkdirRemoteEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->mkdirModeEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->chmodRemoteEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->chmodModeEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);

    connect(ui->selectFetchLocalBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择本地保存目录"), workspace());
        if (!dir.isEmpty()) {
            ui->fetchLocalEdit->setText(dir.endsWith(QLatin1Char('/')) ? dir : dir + QLatin1Char('/'));
        }
    });

    connect(ui->fetchFileBtn, &QPushButton::clicked, this, [this]() {
        const QString remote = ui->fetchRemoteEdit->text().trimmed();
        const QString local = ui->fetchLocalEdit->text().trimmed().isEmpty() ? QStringLiteral("./downloads/") : ui->fetchLocalEdit->text().trimmed();
        if (remote.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入要下载的远端文件路径。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("fetch"), QStringLiteral("src=%1 dest=%2 flat=yes").arg(remote, local)), workspace());
    });
    connect(ui->deleteFileBtn, &QPushButton::clicked, this, [this]() {
        const QString remote = ui->deleteRemoteEdit->text().trimmed();
        if (remote.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入要删除的远端路径。")); return; }
        if (QMessageBox::question(this, QStringLiteral("确认删除"), QStringLiteral("确定要批量删除远端路径 [%1] 吗？此操作不可逆！").arg(remote)) != QMessageBox::Yes) return;
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("file"), QStringLiteral("path=%1 state=absent").arg(remote)), workspace());
    });
    connect(ui->mkdirBtn, &QPushButton::clicked, this, [this]() {
        const QString remote = ui->mkdirRemoteEdit->text().trimmed();
        const QString mode = ui->mkdirModeEdit->text().trimmed().isEmpty() ? QStringLiteral("0755") : ui->mkdirModeEdit->text().trimmed();
        if (remote.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入要创建的远端目录路径。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("file"), QStringLiteral("path=%1 state=directory mode=%2").arg(remote, mode)), workspace());
    });
    connect(ui->chmodBtn, &QPushButton::clicked, this, [this]() {
        const QString remote = ui->chmodRemoteEdit->text().trimmed();
        const QString mode = ui->chmodModeEdit->text().trimmed().isEmpty() ? QStringLiteral("0755") : ui->chmodModeEdit->text().trimmed();
        if (remote.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入要修改权限的远端路径。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("file"), QStringLiteral("path=%1 mode=%2").arg(remote, mode)), workspace());
    });

    // 2. 常用操作 - 软件包
    connect(ui->pkgNameEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->pkgInstallBtn, &QPushButton::clicked, this, [this]() {
        const QString pkg = ui->pkgNameEdit->text().trimmed();
        if (pkg.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入软件包名称。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("package"), QStringLiteral("name=%1 state=present").arg(pkg)), workspace());
    });
    connect(ui->pkgRemoveBtn, &QPushButton::clicked, this, [this]() {
        const QString pkg = ui->pkgNameEdit->text().trimmed();
        if (pkg.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入软件包名称。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("package"), QStringLiteral("name=%1 state=absent").arg(pkg)), workspace());
    });
    connect(ui->pkgAptInstallBtn, &QPushButton::clicked, this, [this]() {
        const QString pkg = ui->pkgNameEdit->text().trimmed();
        if (pkg.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入软件包名称。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("apt"), QStringLiteral("name=%1 state=present update_cache=yes").arg(pkg)), workspace());
    });
    connect(ui->pkgYumInstallBtn, &QPushButton::clicked, this, [this]() {
        const QString pkg = ui->pkgNameEdit->text().trimmed();
        if (pkg.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入软件包名称。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("yum"), QStringLiteral("name=%1 state=present").arg(pkg)), workspace());
    });

    // 3. 批量部署
    connect(ui->playbookCombo, &QComboBox::currentTextChanged, this, [this](const QString &) {
        updatePreview();
        loadPlaybookPreview();
    });
    connect(ui->runPlaybookBtn, &QPushButton::clicked, this, [this]() {
        const QString pb = ui->playbookCombo->currentText().trimmed();
        if (pb.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择有效的 Playbook 剧本文件。")); return; }
        runInExternalTerminal(QStringLiteral("ansible-playbook"), playbookArgs(pb), workspace());
    });
    connect(ui->pingTestBtn, &QPushButton::clicked, this, [this]() {
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("ping")), workspace());
    });
    connect(ui->syntaxCheckBtn, &QPushButton::clicked, this, [this]() {
        const QString pb = ui->playbookCombo->currentText().trimmed();
        if (pb.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个待检查语法的 Playbook 文件。")); return; }
        QStringList args = playbookArgs(pb);
        args << QStringLiteral("--syntax-check");
        runInExternalTerminal(QStringLiteral("ansible-playbook"), args, workspace());
    });

    // 4. 批量命令
    connect(ui->presetCmdCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPresetCmdChanged);
    connect(ui->applyPresetCmdBtn, &QPushButton::clicked, this, &MainWindow::applyPresetCmd);
    connect(ui->moduleCombo, &QComboBox::currentTextChanged, this, &MainWindow::updatePreview);
    connect(ui->batchCmdEdit, &QPlainTextEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->runBatchCmdBtn, &QPushButton::clicked, this, [this]() {
        const QString cmd = ui->batchCmdEdit->toPlainText().trimmed();
        if (cmd.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入要批量执行的命令内容。")); return; }
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(ui->moduleCombo->currentText(), cmd), workspace());
    });

    // 5. 批量上传
    connect(ui->uploadSrcEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->uploadDestEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->browseUploadSrcBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择本地待上传文件"), workspace());
        if (!path.isEmpty()) ui->uploadSrcEdit->setText(path);
    });
    connect(ui->runUploadBtn, &QPushButton::clicked, this, [this]() {
        const QString src = ui->uploadSrcEdit->text().trimmed();
        const QString dest = ui->uploadDestEdit->text().trimmed();
        if (src.isEmpty() || dest.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请完整填写本地文件路径和远端目标路径。")); return; }
        const QString copyArgs = QStringLiteral("src=%1 dest=%2").arg(src, dest);
        runInExternalTerminal(QStringLiteral("ansible"), ansibleArgs(QStringLiteral("copy"), copyArgs), workspace());
    });

    // 6. 日志与停止任务
    connect(ui->clearLogBtn, &QPushButton::clicked, this, &MainWindow::clearLogs);
    connect(ui->copyLogBtn, &QPushButton::clicked, this, &MainWindow::copyLogs);
    connect(ui->exportLogBtn, &QPushButton::clicked, this, &MainWindow::exportLogs);
    connect(ui->searchLogBtn, &QPushButton::clicked, this, [this]() {
        onSearchLog(ui->searchLogEdit->text().trimmed());
    });
    connect(ui->searchLogEdit, &QLineEdit::returnPressed, this, [this]() {
        onSearchLog(ui->searchLogEdit->text().trimmed());
    });

    // 停止当前任务
    connect(ui->stopTaskBtn, &QPushButton::clicked, this, [this]() {
        bool killedAny = false;
        if (process && process->state() != QProcess::NotRunning) {
            process->terminate();
            killedAny = true;
        }
        for (const ExternalRun &run : activeRuns) {
            QFile pidFile(run.pidPath);
            if (pidFile.exists() && pidFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString pidStr = QString::fromUtf8(pidFile.readAll()).trimmed();
                pidFile.close();
                if (!pidStr.isEmpty()) {
                    QProcess::execute(QStringLiteral("kill"), QStringList() << QStringLiteral("-15") << pidStr);
                    QProcess::execute(QStringLiteral("pkill"), QStringList() << QStringLiteral("-P") << pidStr);
                    killedAny = true;
                }
            }
        }
        if (killedAny) {
            appendLog(QStringLiteral("\n[操作提示] 已向正在运行的 Ansible 任务发送终止信号。\n"));
        } else {
            appendLog(QStringLiteral("\n[操作提示] 当前无正在运行的后台任务。\n"));
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 现代、高对比度、纯净优雅的 QSS 样式表（零 Emoji，完美适配各类 Linux 分辨率）
void MainWindow::setupUiStyles()
{
    const QString qss = QStringLiteral(
        "QMainWindow, QWidget#centralwidget { background-color: #f8fafc; font-family: 'Noto Sans CJK SC', 'Microsoft YaHei', 'PingFang SC', sans-serif; font-size: 13px; color: #1e293b; }"
        "QGroupBox { background-color: #ffffff; border: 1px solid #cbd5e1; border-radius: 8px; margin-top: 14px; padding-top: 12px; font-weight: bold; font-size: 13px; color: #0f172a; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 14px; padding: 0 6px; background-color: #f8fafc; color: #0f172a; font-weight: bold; }"
        
        "QLineEdit, QComboBox, QPlainTextEdit { background-color: #ffffff; border: 1px solid #cbd5e1; border-radius: 6px; padding: 5px 8px; color: #0f172a; selection-background-color: #3b82f6; }"
        "QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus { border: 1.5px solid #2563eb; }"
        "QLineEdit[readOnly=\"true\"] { background-color: #f1f5f9; color: #334155; font-family: monospace; font-size: 12px; }"

        "QTabWidget::pane { border: 1px solid #cbd5e1; border-radius: 6px; top: -1px; background-color: #ffffff; }"
        "QTabBar::tab { background-color: #e2e8f0; color: #475569; font-weight: bold; padding: 7px 18px; border: 1px solid #cbd5e1; border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 4px; }"
        "QTabBar::tab:selected { background-color: #ffffff; color: #2563eb; border-top: 2px solid #2563eb; border-bottom: 1px solid #ffffff; }"
        "QTabBar::tab:hover:!selected { background-color: #edf2f7; color: #1e293b; }"

        "QPushButton { background-color: #eff6ff; color: #1d4ed8; font-weight: bold; border: 1px solid #bfdbfe; border-radius: 6px; padding: 6px 14px; min-height: 20px; }"
        "QPushButton:hover { background-color: #dbeafe; border-color: #93c5fd; }"
        "QPushButton:pressed { background-color: #bfdbfe; }"
        "QPushButton:disabled { background-color: #f1f5f9; color: #94a3b8; border-color: #e2e8f0; }"

        "QPushButton#selectInventoryBtn, QPushButton#restartServiceBtn, QPushButton#runPlaybookBtn, QPushButton#runBatchCmdBtn, QPushButton#runUploadBtn, QPushButton#saveInventoryBtn, QPushButton#savePlaybookBtn {"
        "  background-color: #2563eb; color: #ffffff; border: 1px solid #1d4ed8; font-weight: bold;"
        "}"
        "QPushButton#selectInventoryBtn:hover, QPushButton#restartServiceBtn:hover, QPushButton#runPlaybookBtn:hover, QPushButton#runBatchCmdBtn:hover, QPushButton#runUploadBtn:hover, QPushButton#saveInventoryBtn:hover, QPushButton#savePlaybookBtn:hover {"
        "  background-color: #1d4ed8;"
        "}"

        "QPushButton#stopTaskBtn {"
        "  background-color: #fee2e2; color: #dc2626; border: 1px solid #fca5a5; font-size: 13px; font-weight: bold; border-radius: 6px;"
        "}"
        "QPushButton#stopTaskBtn:hover { background-color: #fecaca; border-color: #f87171; }"
        "QPushButton#stopTaskBtn:pressed { background-color: #fca5a5; }"

        "QTableWidget { border: 1px solid #cbd5e1; border-radius: 6px; background-color: #ffffff; gridline-color: #e2e8f0; selection-background-color: #dbeafe; selection-color: #1e293b; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: bold; border: none; border-right: 1px solid #cbd5e1; border-bottom: 1px solid #cbd5e1; padding: 6px; }"
        "QPlainTextEdit#logEdit, QPlainTextEdit#inventoryContentEdit, QPlainTextEdit#playbookContentEdit { background-color: #ffffff; color: #1e293b; font-family: 'Noto Sans Mono', 'DejaVu Sans Mono', 'Consolas', 'Courier New', monospace; font-size: 12px; border: 1px solid #cbd5e1; border-radius: 6px; padding: 8px; selection-background-color: #bfdbfe; selection-color: #0f172a; line-height: 1.4; }"
        "QPushButton#deleteFileBtn { background-color: #fee2e2; color: #dc2626; border: 1px solid #fca5a5; font-weight: bold; border-radius: 6px; }"
        "QPushButton#deleteFileBtn:hover { background-color: #fecaca; border-color: #f87171; }"
    );
    this->setStyleSheet(qss);
}

void MainWindow::onMainTabChanged(int) { updatePreview(); }
void MainWindow::onOpsSubTabChanged(int) { updatePreview(); }
void MainWindow::onInventorySelected(int) { updateInventoryGroups(); updatePreview(); }

void MainWindow::clearLogs() { ui->logEdit->clear(); }

void MainWindow::copyLogs()
{
    QApplication::clipboard()->setText(ui->logEdit->toPlainText());
    QMessageBox::information(this, QStringLiteral("已复制"), QStringLiteral("控制台全部日志已复制到剪贴板。"));
}

void MainWindow::exportLogs()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出日志文件"),
        QDir::homePath() + QStringLiteral("/ansible-pilot-%1.log").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"))),
        QStringLiteral("日志文件 (*.log);;所有文件 (*)")
    );
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << ui->logEdit->toPlainText();
        file.close();
        QMessageBox::information(this, QStringLiteral("导出成功"), QStringLiteral("日志已成功保存至:\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, QStringLiteral("导出失败"), QStringLiteral("无法写入目标文件，请检查目录权限。"));
    }
}

void MainWindow::onSearchLog(const QString &keyword)
{
    if (keyword.isEmpty()) return;

    bool found = ui->logEdit->find(keyword);
    if (!found) {
        QTextCursor cursor = ui->logEdit->textCursor();
        cursor.movePosition(QTextCursor::Start);
        ui->logEdit->setTextCursor(cursor);
        found = ui->logEdit->find(keyword);
        if (!found) {
            QMessageBox::information(this, QStringLiteral("搜索结果"), QStringLiteral("未在日志中找到匹配的文本: \"%1\"").arg(keyword));
        }
    }
}

void MainWindow::updateInventoryGroups()
{
    const QString currentInventory = inventory();
    if (currentInventory.isEmpty()) return;

    QString hostFilePath = currentInventory;
    if (!QDir::isAbsolutePath(hostFilePath)) {
        hostFilePath = QDir(workspace()).filePath(hostFilePath);
    }

    QFile file(hostFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QString currentGroup = ui->hostGroupCombo->currentText();
    QStringList groups = { QStringLiteral("all"), QStringLiteral("ungrouped") };

    QRegularExpression groupRegex(QStringLiteral("^\\[([a-zA-Z0-9_\\-\\.:]+)\\]"));
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        auto match = groupRegex.match(line);
        if (match.hasMatch()) {
            const QString rawName = match.captured(1);
            if (!rawName.endsWith(QStringLiteral(":vars")) && !rawName.endsWith(QStringLiteral(":children"))) {
                if (!groups.contains(rawName)) {
                    groups.append(rawName);
                }
            }
        }
    }
    file.close();

    ui->hostGroupCombo->blockSignals(true);
    ui->hostGroupCombo->clear();
    ui->hostGroupCombo->addItems(groups);
    int idx = ui->hostGroupCombo->findText(currentGroup);
    if (idx >= 0) {
        ui->hostGroupCombo->setCurrentIndex(idx);
    } else if (ui->hostGroupCombo->count() > 0) {
        ui->hostGroupCombo->setCurrentIndex(0);
    }
    ui->hostGroupCombo->blockSignals(false);
    loadInventoryPreview();
}

void MainWindow::scanFiles()
{
    const QDir dir(workspace());
    if (!dir.exists()) return;

    const QString oldInv = ui->inventoryCombo->currentText().trimmed();
    const QString oldPb = ui->playbookCombo->currentText().trimmed();

    const bool invBlocked = ui->inventoryCombo->blockSignals(true);
    const bool pbBlocked = ui->playbookCombo->blockSignals(true);

    ui->inventoryCombo->clear();
    ui->playbookCombo->clear();

    QStringList foundInventories;

    // 1. 系统默认主机清单
    if (QFile::exists(QStringLiteral("/etc/ansible/hosts"))) {
        foundInventories.append(QStringLiteral("/etc/ansible/hosts"));
    }

    // 2. 动态扫描工作空间目录下所有主机清单文件 (INI / 文本格式 / 命名包含 hosts 或 inventory)
    const QStringList invFilters = {
        QStringLiteral("*hosts*"),
        QStringLiteral("*inventory*"),
        QStringLiteral("*.ini"),
        QStringLiteral("*.cfg"),
        QStringLiteral("*.txt")
    };
    const QStringList scannedFiles = dir.entryList(invFilters, QDir::Files | QDir::Readable, QDir::Name);
    for (const QString &f : scannedFiles) {
        // 过滤掉日志、临时文件以及剧本文件
        if (f.endsWith(QStringLiteral(".log")) || f.endsWith(QStringLiteral(".tmp")) || 
            f.endsWith(QStringLiteral(".yaml")) || f.endsWith(QStringLiteral(".yml")) ||
            f == QStringLiteral("cluster_config.xml")) {
            continue;
        }
        const QString fullPath = dir.filePath(f);
        if (!foundInventories.contains(fullPath)) {
            foundInventories.append(fullPath);
        }
    }

    // 3. 常规候选补充
    const QStringList inventoryCandidates = {
        QStringLiteral("my_hosts"),
        QStringLiteral("hosts"),
        QStringLiteral("inventory"),
        QStringLiteral("inventory.ini")
    };
    for (const QString &name : inventoryCandidates) {
        const QString fullPath = dir.filePath(name);
        if (dir.exists(name) && !foundInventories.contains(fullPath)) {
            foundInventories.append(fullPath);
        }
    }

    // 如果之前选中的 inventory 存在但不在扫描列表中，保留并加入
    if (!oldInv.isEmpty() && !foundInventories.contains(oldInv)) {
        if (QFile::exists(oldInv)) {
            foundInventories.append(oldInv);
        }
    }

    if (foundInventories.isEmpty()) {
        foundInventories.append(dir.filePath(QStringLiteral("my_hosts")));
    }

    for (const QString &invPath : foundInventories) {
        ui->inventoryCombo->addItem(invPath);
    }

    // 4. 扫描 Playbook (*.yaml, *.yml)
    const QStringList pbEntries = dir.entryList(
        QStringList() << QStringLiteral("*.yaml") << QStringLiteral("*.yml"),
        QDir::Files | QDir::Readable,
        QDir::Name
    );
    QStringList foundPlaybooks;
    for (const QString &name : pbEntries) {
        if (name != QStringLiteral("cluster_config.xml")) {
            foundPlaybooks.append(dir.filePath(name));
        }
    }

    if (!oldPb.isEmpty() && !foundPlaybooks.contains(oldPb)) {
        if (QFile::exists(oldPb)) {
            foundPlaybooks.append(oldPb);
        }
    }

    if (foundPlaybooks.isEmpty()) {
        foundPlaybooks.append(dir.filePath(QStringLiteral("ansible.yaml")));
    }

    for (const QString &pbPath : foundPlaybooks) {
        ui->playbookCombo->addItem(pbPath);
    }

    // 5. 恢复之前选中的 Inventory，确保刷新扫描时不被覆盖重置
    int invIdx = -1;
    if (!oldInv.isEmpty()) {
        invIdx = ui->inventoryCombo->findText(oldInv);
    }
    if (invIdx >= 0) {
        ui->inventoryCombo->setCurrentIndex(invIdx);
    } else if (ui->inventoryCombo->count() > 0) {
        ui->inventoryCombo->setCurrentIndex(0);
    }

    // 6. 恢复之前选中的 Playbook
    int pbIdx = -1;
    if (!oldPb.isEmpty()) {
        pbIdx = ui->playbookCombo->findText(oldPb);
    }
    if (pbIdx >= 0) {
        ui->playbookCombo->setCurrentIndex(pbIdx);
    } else if (ui->playbookCombo->count() > 0) {
        ui->playbookCombo->setCurrentIndex(0);
    }

    ui->inventoryCombo->blockSignals(invBlocked);
    ui->playbookCombo->blockSignals(pbBlocked);

    loadClusterConfig();
    loadInventoryPreview();
    loadPlaybookPreview();
}

QString MainWindow::ansiToHtml(const QString &text) const
{
    QString html = text.toHtmlEscaped();

    html.replace(QRegularExpression(QStringLiteral("\033\\[0?32m")), QStringLiteral("<span style=\"color:#15803d;font-weight:bold;\">"));
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?31m")), QStringLiteral("<span style=\"color:#dc2626;font-weight:bold;\">"));
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?33m")), QStringLiteral("<span style=\"color:#b45309;font-weight:bold;\">"));
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?36m")), QStringLiteral("<span style=\"color:#0369a1;font-weight:bold;\">"));
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?34m")), QStringLiteral("<span style=\"color:#1d4ed8;font-weight:bold;\">"));
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?35m")), QStringLiteral("<span style=\"color:#7e22ce;font-weight:bold;\">"));
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?1m")),  QStringLiteral("<span style=\"font-weight:bold;\">"));
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?m")),   QStringLiteral("</span>"));
    html.replace(QRegularExpression(QStringLiteral("\033\\[[0-9;]*m")), QString());

    html.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    html.replace(QStringLiteral("  "), QStringLiteral("&nbsp;&nbsp;"));
    return html;
}

void MainWindow::appendLog(const QString &message)
{
    if (message.isEmpty()) return;

    QTextCursor cursor(ui->logEdit->document());
    cursor.movePosition(QTextCursor::End);

    if (message.contains(QLatin1Char('\033'))) {
        cursor.insertHtml(ansiToHtml(message));
    } else {
        cursor.insertText(message);
    }

    if (ui->autoScrollCheck->isChecked()) {
        ui->logEdit->setTextCursor(cursor);
        ui->logEdit->ensureCursorVisible();
    }
}

void MainWindow::updatePreview()
{
    QString preview;
    const int mainTab = ui->mainTabWidget->currentIndex();

    if (mainTab == 0) { // 常用操作
        const int subTab = ui->opsSubTabWidget->currentIndex();
        if (subTab == 0) { // 服务管理
            const QString s = ui->serviceNameEdit->text().trimmed();
            preview = QStringLiteral("ansible -i %1 %2 %3 -m systemd -a 'name=%4 state=restarted'")
                          .arg(inventory(), group(), limit().isEmpty() ? QString() : QStringLiteral("-l ") + limit(), s.isEmpty() ? QStringLiteral("sshd") : s);
        } else if (subTab == 1) { // 系统信息
            preview = QStringLiteral("ansible -i %1 %2 %3 -m command -a 'df -h'")
                          .arg(inventory(), group(), limit().isEmpty() ? QString() : QStringLiteral("-l ") + limit());
        } else if (subTab == 2) { // 进程与日志
            const QString kw = ui->procKeywordEdit->text().trimmed();
            preview = QStringLiteral("ansible -i %1 %2 %3 -m shell -a 'ps -ef | grep %4'")
                          .arg(inventory(), group(), limit().isEmpty() ? QString() : QStringLiteral("-l ") + limit(), kw.isEmpty() ? QStringLiteral("nginx") : kw);
        } else if (subTab == 3) { // 文件操作
            const QString fr = ui->fetchRemoteEdit->text().trimmed();
            const QString fl = ui->fetchLocalEdit->text().trimmed().isEmpty() ? QStringLiteral("./downloads/") : ui->fetchLocalEdit->text().trimmed();
            preview = QStringLiteral("ansible -i %1 %2 %3 -m fetch -a 'src=%4 dest=%5 flat=yes'")
                          .arg(inventory(), group(), limit().isEmpty() ? QString() : QStringLiteral("-l ") + limit(), fr.isEmpty() ? QStringLiteral("/var/log/messages") : fr, fl);
        } else if (subTab == 4) { // 软件包
            const QString pkg = ui->pkgNameEdit->text().trimmed();
            preview = QStringLiteral("ansible -i %1 %2 %3 -m package -a 'name=%4 state=present'")
                          .arg(inventory(), group(), limit().isEmpty() ? QString() : QStringLiteral("-l ") + limit(), pkg.isEmpty() ? QStringLiteral("vim") : pkg);
        }
    } else if (mainTab == 1) { // 批量部署
        const QString pb = ui->playbookCombo->currentText().trimmed();
        preview = QStringLiteral("Playbook: ansible-playbook -i %1 %2 %3 %4")
                      .arg(inventory(), pb, limit().isEmpty() ? QString() : QStringLiteral("-l ") + limit(), extraArgs());
    } else if (mainTab == 2) { // 批量命令
        const QString cmd = ui->batchCmdEdit->toPlainText().trimmed();
        preview = QStringLiteral("ansible -i %1 %2 %3 -m %4 -a '%5'")
                      .arg(inventory(), group(), limit().isEmpty() ? QString() : QStringLiteral("-l ") + limit(), ui->moduleCombo->currentText(), cmd.isEmpty() ? QStringLiteral("<command>") : cmd);
    } else if (mainTab == 3) { // 批量上传
        const QString src = ui->uploadSrcEdit->text().trimmed();
        const QString dest = ui->uploadDestEdit->text().trimmed();
        preview = QStringLiteral("ansible -i %1 %2 %3 -m copy -a 'src=%4 dest=%5'")
                      .arg(inventory(), group(), limit().isEmpty() ? QString() : QStringLiteral("-l ") + limit(), src.isEmpty() ? QStringLiteral("<src>") : src, dest.isEmpty() ? QStringLiteral("<dest>") : dest);
    }

    ui->previewEdit->setText(preview.simplified());
}

QString MainWindow::workspace() const { return ui->workDirEdit->text().trimmed(); }
QString MainWindow::inventory() const { return ui->inventoryCombo->currentText().trimmed(); }
QString MainWindow::group() const { return ui->hostGroupCombo->currentText().trimmed(); }
QString MainWindow::limit() const { return ui->limitEdit->text().trimmed(); }
QString MainWindow::extraArgs() const { return ui->extraArgsEdit->text().trimmed(); }

QStringList MainWindow::commonArgs() const
{
    QStringList args;
    const QString inv = inventory();
    if (!inv.isEmpty()) args << QStringLiteral("-i") << inv;
    const QString lim = limit();
    if (!lim.isEmpty()) args << QStringLiteral("-l") << lim;
    const QString extra = extraArgs();
    if (!extra.isEmpty()) args << splitArgs(extra);
    return args;
}

QStringList MainWindow::ansibleArgs(const QString &module, const QString &moduleArgs) const
{
    QStringList args;
    const QString inv = inventory();
    if (!inv.isEmpty()) args << QStringLiteral("-i") << inv;
    args << (group().isEmpty() ? QStringLiteral("all") : group());
    if (!module.isEmpty()) {
        args << QStringLiteral("-m") << module;
        if (!moduleArgs.isEmpty()) {
            args << QStringLiteral("-a") << moduleArgs;
        }
    }
    const QString lim = limit();
    if (!lim.isEmpty()) args << QStringLiteral("-l") << lim;
    const QString extra = extraArgs();
    if (!extra.isEmpty()) args << splitArgs(extra);
    return args;
}

QStringList MainWindow::playbookArgs(const QString &playbook) const
{
    QStringList args;
    const QString inv = inventory();
    if (!inv.isEmpty()) args << QStringLiteral("-i") << inv;
    args << playbook;
    const QString lim = limit();
    if (!lim.isEmpty()) args << QStringLiteral("-l") << lim;
    const QString extra = extraArgs();
    if (!extra.isEmpty()) args << splitArgs(extra);
    return args;
}

QStringList MainWindow::splitArgs(const QString &text) const
{
    QStringList result;
    QString current;
    bool inQuote = false;
    QChar quoteChar;

    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if ((c == QLatin1Char('\'') || c == QLatin1Char('"')) && !inQuote) {
            inQuote = true;
            quoteChar = c;
            continue;
        }
        if (inQuote && c == quoteChar) {
            inQuote = false;
            continue;
        }
        if (c.isSpace() && !inQuote) {
            if (!current.isEmpty()) {
                result << current;
                current.clear();
            }
            continue;
        }
        current.append(c);
    }
    if (!current.isEmpty()) result << current;
    return result;
}

QString MainWindow::shellQuote(const QString &text) const
{
    QString quoted = text;
    quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'%1'").arg(quoted);
}

void MainWindow::cleanupOldTempFiles()
{
    QDir tempDir(QDir::tempPath());
    QStringList oldFiles = tempDir.entryList(QStringList() << QStringLiteral("ansible-pilot-*"), QDir::Files);
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-1);

    for (const QString &file : oldFiles) {
        QFileInfo fi(tempDir.filePath(file));
        if (fi.lastModified() < cutoff) {
            tempDir.remove(file);
        }
    }
}

int MainWindow::recordHistoryStart(const QString &command)
{
    const int row = ui->historyTable->rowCount();
    ui->historyTable->insertRow(row);

    const QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    ui->historyTable->setItem(row, 0, new QTableWidgetItem(timeStr));
    ui->historyTable->setItem(row, 1, new QTableWidgetItem(command));

    auto *statusItem = new QTableWidgetItem(QStringLiteral("运行中"));
    statusItem->setForeground(QBrush(QColor(37, 99, 235)));
    ui->historyTable->setItem(row, 2, statusItem);

    ui->historyTable->setItem(row, 3, new QTableWidgetItem(QStringLiteral("-")));
    ui->historyTable->setItem(row, 4, new QTableWidgetItem(QStringLiteral("计时中...")));
    ui->historyTable->setItem(row, 5, new QTableWidgetItem(QStringLiteral("执行中")));

    ui->historyTable->scrollToBottom();
    return row;
}

void MainWindow::recordHistoryFinish(int row, const QString &status, const QString &exitCode, qint64 durationMs)
{
    if (row < 0 || row >= ui->historyTable->rowCount()) return;

    auto *statusItem = ui->historyTable->item(row, 2);
    if (statusItem) {
        statusItem->setText(status);
        if (status == QStringLiteral("成功")) {
            statusItem->setForeground(QBrush(QColor(22, 163, 74)));
        } else {
            statusItem->setForeground(QBrush(QColor(220, 38, 38)));
        }
    }

    auto *codeItem = ui->historyTable->item(row, 3);
    if (codeItem) codeItem->setText(exitCode);

    auto *durItem = ui->historyTable->item(row, 4);
    if (durItem) {
        if (durationMs > 0) {
            double sec = durationMs / 1000.0;
            durItem->setText(QStringLiteral("%1s").arg(sec, 0, 'f', 2));
        } else {
            durItem->setText(QStringLiteral("< 1s"));
        }
    }

    auto *actionItem = ui->historyTable->item(row, 5);
    if (actionItem) actionItem->setText(QStringLiteral("已完成"));
}

void MainWindow::runProcess(const QString &program, const QStringList &arguments, const QString &workingDirectory)
{
    if (process->state() != QProcess::NotRunning) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("当前有正在运行的内置任务，请先点击停止当前任务。"));
        return;
    }

    const QString fullCommand = program + QLatin1Char(' ') + arguments.join(QLatin1Char(' '));
    const int historyRow = recordHistoryStart(fullCommand);
    const qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    appendLog(QStringLiteral("\n------------------------------------------------------------\n"));
    appendLog(QStringLiteral("[启动任务] 工作目录: %1\n").arg(workingDirectory));
    appendLog(QStringLiteral("[完整指令] %1\n").arg(fullCommand));
    appendLog(QStringLiteral("------------------------------------------------------------\n"));

    process->setWorkingDirectory(workingDirectory);
    
    disconnect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), nullptr, nullptr);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, historyRow, startTime](int exitCode, QProcess::ExitStatus) {
        qint64 durationMs = QDateTime::currentMSecsSinceEpoch() - startTime;
        appendLog(QStringLiteral("\n[任务执行完成] 退出代码: %1 (耗时 %2 秒)\n").arg(exitCode).arg(durationMs / 1000.0, 0, 'f', 2));
        recordHistoryFinish(historyRow, exitCode == 0 ? QStringLiteral("成功") : QStringLiteral("失败"), QString::number(exitCode), durationMs);
    });

    process->start(program, arguments);
}

void MainWindow::runInExternalTerminal(const QString &program, const QStringList &arguments, const QString &workingDirectory)
{
    const QString token = QString::number(QDateTime::currentMSecsSinceEpoch());
    const QString scriptPath = QDir::temp().filePath(QStringLiteral("ansible-pilot-run-%1.sh").arg(token));
    const QString logPath = QDir::temp().filePath(QStringLiteral("ansible-pilot-output-%1.log").arg(token));
    const QString statusPath = QDir::temp().filePath(QStringLiteral("ansible-pilot-%1.status").arg(token));
    const QString pidPath = QDir::temp().filePath(QStringLiteral("ansible-pilot-%1.pid").arg(token));

    QStringList tokenList;
    tokenList << shellQuote(program);
    for (const QString &arg : arguments) {
        tokenList << shellQuote(arg);
    }
    const QString execCommandStr = tokenList.join(QLatin1Char(' '));
    const QString displayCmd = program + QLatin1Char(' ') + arguments.join(QLatin1Char(' '));

    const int historyRow = recordHistoryStart(displayCmd);

    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        appendLog(QStringLiteral("[错误] 无法创建临时脚本: %1\n").arg(scriptPath));
        recordHistoryFinish(historyRow, QStringLiteral("失败"), QStringLiteral("-1"));
        return;
    }

    QString scriptContent;
    scriptContent += QStringLiteral("#!/usr/bin/env bash\n");
    scriptContent += QStringLiteral("export LANG=\"${LANG:-zh_CN.UTF-8}\"\n");
    scriptContent += QStringLiteral("export LC_ALL=\"${LC_ALL:-zh_CN.UTF-8}\"\n");
    scriptContent += QStringLiteral("cd ") + shellQuote(workingDirectory) + QStringLiteral(" || exit 1\n");
    scriptContent += QStringLiteral("echo $$ > ") + shellQuote(pidPath) + QStringLiteral("\n");
    scriptContent += QStringLiteral(": > ") + shellQuote(logPath) + QStringLiteral("\n\n");
    scriptContent += QStringLiteral("echo \"========== AnsiblePilot 终端执行 ==========\"\n");
    scriptContent += QStringLiteral("echo \"工作目录：$(pwd)\"\n");
    scriptContent += QStringLiteral("echo \"当前用户：$(whoami)\"\n");
    scriptContent += QStringLiteral("echo \"PID：$$\"\n");
    scriptContent += QStringLiteral("echo \"---------- Ansible 命令 ----------\"\n");
    scriptContent += QStringLiteral("echo ") + shellQuote(displayCmd) + QStringLiteral("\n");
    scriptContent += QStringLiteral("echo \"\"\n\n");
    scriptContent += QStringLiteral("export ANSIBLE_FORCE_COLOR=true\n");
    scriptContent += QStringLiteral("export PYTHONUNBUFFERED=1\n\n");
    scriptContent += execCommandStr + QStringLiteral(" 2>&1 | tee -a ") + shellQuote(logPath) + QStringLiteral("\n");
    scriptContent += QStringLiteral("EXIT_CODE=${PIPESTATUS[0]}\n");
    scriptContent += QStringLiteral("echo $EXIT_CODE > ") + shellQuote(statusPath) + QStringLiteral("\n\n");
    scriptContent += QStringLiteral("echo \"\"\n");
    scriptContent += QStringLiteral("echo \"任务结束，退出码：$EXIT_CODE\"\n");
    scriptContent += QStringLiteral("echo \"按回车关闭终端...\"\n");
    scriptContent += QStringLiteral("read -r\n");

    scriptFile.write(scriptContent.toUtf8());
    scriptFile.close();

    QFile::setPermissions(scriptPath,
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                          QFile::ReadGroup | QFile::ExeGroup |
                          QFile::ReadOther | QFile::ExeOther);

    const QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    appendLog(QStringLiteral("[%1] 在外部终端执行：%2\n").arg(timeStr, execCommandStr));
    appendLog(QStringLiteral("终端脚本：%1\n").arg(scriptPath));
    appendLog(QStringLiteral("同步日志：%1\n").arg(logPath));

    ExternalRun run;
    run.logPath = logPath;
    run.statusPath = statusPath;
    run.pidPath = pidPath;
    run.logOffset = 0;
    run.historyRow = historyRow;
    run.startTime = QDateTime::currentDateTime();
    activeRuns.append(run);

    QString openedTerminal;
    if (QProcess::startDetached(QStringLiteral("x-terminal-emulator"), QStringList() << QStringLiteral("-e") << scriptPath)) {
        openedTerminal = QStringLiteral("x-terminal-emulator");
    } else if (QProcess::startDetached(QStringLiteral("mate-terminal"), QStringList() << QStringLiteral("-e") << scriptPath)) {
        openedTerminal = QStringLiteral("mate-terminal");
    } else if (QProcess::startDetached(QStringLiteral("gnome-terminal"), QStringList() << QStringLiteral("--") << scriptPath)) {
        openedTerminal = QStringLiteral("gnome-terminal");
    } else if (QProcess::startDetached(QStringLiteral("kylin-terminal"), QStringList() << QStringLiteral("-e") << scriptPath)) {
        openedTerminal = QStringLiteral("kylin-terminal");
    } else if (QProcess::startDetached(QStringLiteral("konsole"), QStringList() << QStringLiteral("-e") << scriptPath)) {
        openedTerminal = QStringLiteral("konsole");
    } else if (QProcess::startDetached(QStringLiteral("xterm"), QStringList() << QStringLiteral("-e") << scriptPath)) {
        openedTerminal = QStringLiteral("xterm");
    } else {
        appendLog(QStringLiteral("[提示] 未检测到独立图形终端，已自动切换至内置捕获模式执行。\n"));
        runProcess(QStringLiteral("bash"), QStringList() << scriptPath, workingDirectory);
    }

    if (!openedTerminal.isEmpty()) {
        appendLog(QStringLiteral("已打开外部终端：%1\n").arg(openedTerminal));
    }
}

void MainWindow::pollExternalTerminalOutput()
{
    if (activeRuns.isEmpty()) return;

    for (int i = activeRuns.size() - 1; i >= 0; --i) {
        ExternalRun &run = activeRuns[i];

        QFile logFile(run.logPath);
        if (logFile.exists() && logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (logFile.size() > run.logOffset) {
                logFile.seek(run.logOffset);
                const QString chunk = QString::fromUtf8(logFile.readAll());
                run.logOffset = logFile.pos();
                appendLog(chunk);
            }
            logFile.close();
        }

        QFile statusFile(run.statusPath);
        if (statusFile.exists() && statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString exitCode = QString::fromUtf8(statusFile.readAll()).trimmed();
            statusFile.close();

            qint64 durationMs = run.startTime.msecsTo(QDateTime::currentDateTime());
            appendLog(QStringLiteral("\n外部终端任务结束，退出码：%1\n").arg(exitCode));
            recordHistoryFinish(run.historyRow, exitCode == QStringLiteral("0") ? QStringLiteral("成功") : QStringLiteral("失败"), exitCode, durationMs);

            activeRuns.removeAt(i);
        }
    }
}

void MainWindow::loadClusterConfig()
{
    pairInfos.clear();
    processPresets.clear();
    commandPresets.clear();

    const QString ws = workspace();
    const QStringList candidatePaths = {
        ws + QStringLiteral("/config/cluster_config.xml"),
        ws + QStringLiteral("/cluster_config.xml"),
        QDir::currentPath() + QStringLiteral("/config/cluster_config.xml"),
        QDir::currentPath() + QStringLiteral("/cluster_config.xml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/config/cluster_config.xml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../config/cluster_config.xml"),
        QStringLiteral("config/cluster_config.xml")
    };

    QString configFile;
    for (const QString &p : candidatePaths) {
        if (QFile::exists(p)) {
            configFile = p;
            break;
        }
    }

    if (!configFile.isEmpty()) {
        appendLog(QStringLiteral("[配置系统] 已加载集群配置文件: %1\n").arg(configFile));
        QFile file(configFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QXmlStreamReader xml(&file);
            QString currentCategory;

            while (!xml.atEnd() && !xml.hasError()) {
                xml.readNext();
                if (xml.isStartElement()) {
                    const QString tag = xml.name().toString();
                    if (tag == QStringLiteral("PairIdInfo")) {
                        PairInfo info;
                        while (!(xml.isEndElement() && xml.name().toString() == QStringLiteral("PairIdInfo")) && !xml.atEnd()) {
                            xml.readNext();
                            if (xml.isStartElement()) {
                                const QString subTag = xml.name().toString();
                                const QString text = xml.readElementText();
                                if (subTag == QStringLiteral("pairid")) info.pairId = text.toInt();
                                else if (subTag == QStringLiteral("servicename")) info.serviceName = text;
                                else if (subTag == QStringLiteral("softwarename")) info.softwareName = text;
                                else if (subTag == QStringLiteral("masterIp")) info.masterIp = text;
                                else if (subTag == QStringLiteral("slaveIp")) info.slaveIp = text;
                            }
                        }
                        if (info.pairId > 0) pairInfos.append(info);
                    } else if (tag == QStringLiteral("Process")) {
                        const QXmlStreamAttributes attrs = xml.attributes();
                        ProcessPreset proc;
                        proc.name = attrs.value(QStringLiteral("name")).toString();
                        proc.keyword = attrs.value(QStringLiteral("keyword")).toString();
                        proc.logPath = attrs.value(QStringLiteral("logPath")).toString();
                        proc.pairId = attrs.value(QStringLiteral("pairId")).toInt();
                        proc.desc = attrs.value(QStringLiteral("desc")).toString();

                        while (!(xml.isEndElement() && xml.name().toString() == QStringLiteral("Process")) && !xml.atEnd()) {
                            xml.readNext();
                            if (xml.isStartElement() && xml.name().toString() == QStringLiteral("pidQuery")) {
                                proc.pidQuery = xml.readElementText();
                            }
                        }
                        processPresets.append(proc);
                    } else if (tag == QStringLiteral("Category")) {
                        currentCategory = xml.attributes().value(QStringLiteral("name")).toString();
                    } else if (tag == QStringLiteral("Command")) {
                        const QXmlStreamAttributes attrs = xml.attributes();
                        CommandPreset cmd;
                        cmd.category = currentCategory.isEmpty() ? QStringLiteral("常用命令") : currentCategory;
                        cmd.name = attrs.value(QStringLiteral("name")).toString();
                        cmd.script = attrs.value(QStringLiteral("script")).toString();
                        cmd.target = attrs.value(QStringLiteral("target")).toString();
                        cmd.module = attrs.hasAttribute(QStringLiteral("module")) ? attrs.value(QStringLiteral("module")).toString() : QStringLiteral("shell");
                        cmd.desc = attrs.value(QStringLiteral("desc")).toString();
                        cmd.pairId = attrs.value(QStringLiteral("pairId")).toInt();
                        commandPresets.append(cmd);
                    }
                }
            }
            file.close();
        }
    }

    // 填充到「进程与日志」预设下拉列表
    const bool procBlocked = ui->presetProcCombo->blockSignals(true);
    ui->presetProcCombo->clear();
    ui->presetProcCombo->addItem(QStringLiteral("-- 自定义输入 (或选择预设快速填入) --"));
    for (const auto &p : processPresets) {
        QString pairText;
        for (const auto &pair : pairInfos) {
            if (pair.pairId == p.pairId) {
                pairText = QStringLiteral("[热备%1: %2]").arg(QString::number(pair.pairId), pair.serviceName);
                break;
            }
        }
        if (pairText.isEmpty()) {
            pairText = p.pairId > 0 ? QStringLiteral("[热备%1]").arg(p.pairId) : QStringLiteral("[通用]");
        }
        QString label = QStringLiteral("%1 %2 (进程: %3 | 日志: %4)").arg(pairText, p.name, p.keyword, p.logPath);
        ui->presetProcCombo->addItem(label);
    }
    ui->presetProcCombo->blockSignals(procBlocked);

    // 填充到「批量命令」预设下拉列表
    const bool cmdBlocked = ui->presetCmdCombo->blockSignals(true);
    ui->presetCmdCombo->clear();
    ui->presetCmdCombo->addItem(QStringLiteral("-- 自定义输入 (或选择预设快速填入) --"));
    for (const auto &c : commandPresets) {
        QString pairText;
        if (c.pairId > 0) {
            for (const auto &pair : pairInfos) {
                if (pair.pairId == c.pairId) {
                    pairText = QStringLiteral(" [热备%1: %2]").arg(QString::number(pair.pairId), pair.serviceName);
                    break;
                }
            }
            if (pairText.isEmpty()) {
                pairText = QStringLiteral(" [热备%1]").arg(c.pairId);
            }
        }
        QString label = QStringLiteral("【%1】%2 (%3)%4").arg(c.category, c.name, c.script, pairText);
        ui->presetCmdCombo->addItem(label);
    }
    ui->presetCmdCombo->blockSignals(cmdBlocked);
}

void MainWindow::onPresetProcChanged(int index)
{
    if (index <= 0 || index - 1 >= processPresets.size()) return;
    const ProcessPreset &p = processPresets.at(index - 1);
    ui->procKeywordEdit->setText(p.keyword);
    ui->logPathEdit->setText(p.logPath);
    updatePreview();
}

void MainWindow::applyPresetProc()
{
    int index = ui->presetProcCombo->currentIndex();
    if (index <= 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先在下拉列表中选择一个预设进程或日志项。"));
        return;
    }
    const ProcessPreset &p = processPresets.at(index - 1);
    ui->procKeywordEdit->setText(p.keyword);
    ui->logPathEdit->setText(p.logPath);

    // 如果对应了热备对，自动填入目标主机的 IP 到 Limit 框中
    if (p.pairId > 0) {
        for (const auto &pair : pairInfos) {
            if (pair.pairId == p.pairId) {
                QString targetLimit;
                if (!pair.masterIp.isEmpty() && !pair.slaveIp.isEmpty()) {
                    targetLimit = QStringLiteral("%1,%2").arg(pair.masterIp, pair.slaveIp);
                } else if (!pair.masterIp.isEmpty()) {
                    targetLimit = pair.masterIp;
                }
                if (!targetLimit.isEmpty()) {
                    ui->limitEdit->setText(targetLimit);
                    appendLog(QStringLiteral("[配置提示] 已设置目标热备对 Limit: %1 (%2)\n").arg(targetLimit, pair.serviceName));
                }
                break;
            }
        }
    }
    updatePreview();
}

void MainWindow::onPresetCmdChanged(int index)
{
    if (index <= 0 || index - 1 >= commandPresets.size()) return;
    const CommandPreset &c = commandPresets.at(index - 1);
    ui->batchCmdEdit->setPlainText(c.script);
    int modIdx = ui->moduleCombo->findText(c.module);
    if (modIdx >= 0) ui->moduleCombo->setCurrentIndex(modIdx);
    updatePreview();
}

void MainWindow::applyPresetCmd()
{
    int index = ui->presetCmdCombo->currentIndex();
    if (index <= 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先在下拉列表中选择一个预设脚本或命令。"));
        return;
    }
    const CommandPreset &c = commandPresets.at(index - 1);
    ui->batchCmdEdit->setPlainText(c.script);
    int modIdx = ui->moduleCombo->findText(c.module);
    if (modIdx >= 0) ui->moduleCombo->setCurrentIndex(modIdx);

    // 如果该命令对应了特定热备对，自动填入目标主机的 IP
    if (c.pairId > 0) {
        for (const auto &pair : pairInfos) {
            if (pair.pairId == c.pairId) {
                QString targetLimit;
                if (!pair.masterIp.isEmpty() && !pair.slaveIp.isEmpty()) {
                    targetLimit = QStringLiteral("%1,%2").arg(pair.masterIp, pair.slaveIp);
                } else if (!pair.masterIp.isEmpty()) {
                    targetLimit = pair.masterIp;
                }
                if (!targetLimit.isEmpty()) {
                    ui->limitEdit->setText(targetLimit);
                    appendLog(QStringLiteral("[配置提示] 已设置目标热备对 Limit: %1 (%2)\n").arg(targetLimit, pair.serviceName));
                }
                break;
            }
        }
    }
    updatePreview();
}

void MainWindow::openInventoryPreviewTab()
{
    loadInventoryPreview();
    ui->logTabWidget->setCurrentWidget(ui->tabInventoryPreview);
}

void MainWindow::loadInventoryPreview()
{
    const QString currentInventory = inventory();
    if (currentInventory.isEmpty()) {
        ui->labelInvPreviewFile->setText(QStringLiteral("当前主机清单: (未选择主机清单文件)"));
        ui->inventoryContentEdit->setPlaceholderText(QStringLiteral("请在上方「基础配置」中指定或选择一个主机清单 (Inventory) 文件"));
        ui->inventoryContentEdit->clear();
        return;
    }

    QString hostFilePath = currentInventory;
    if (!QDir::isAbsolutePath(hostFilePath)) {
        hostFilePath = QDir(workspace()).filePath(hostFilePath);
    }

    QFile file(hostFilePath);
    if (!file.exists()) {
        ui->labelInvPreviewFile->setText(QStringLiteral("当前主机清单: %1 (文件不存在)").arg(hostFilePath));
        ui->inventoryContentEdit->clear();
        ui->inventoryContentEdit->setPlaceholderText(QStringLiteral("文件不存在: %1\n您可以直接在此处编写主机清单内容并点击「保存修改」新建此文件。").arg(hostFilePath));
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->labelInvPreviewFile->setText(QStringLiteral("当前主机清单: %1 (无法读取，权限不足)").arg(hostFilePath));
        return;
    }

    QTextStream in(&file);
    const QString content = in.readAll();
    file.close();

    const int lineCount = content.count(QLatin1Char('\n')) + (content.isEmpty() ? 0 : 1);
    ui->labelInvPreviewFile->setText(QStringLiteral("当前主机清单: %1 (共 %2 行, 大小 %3 字节)").arg(hostFilePath).arg(lineCount).arg(content.toUtf8().size()));
    ui->inventoryContentEdit->setPlainText(content);
}

void MainWindow::saveInventoryPreview()
{
    const QString currentInventory = inventory();
    if (currentInventory.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先在上方「基础配置」中指定有效的主机清单文件路径。"));
        return;
    }

    QString hostFilePath = currentInventory;
    if (!QDir::isAbsolutePath(hostFilePath)) {
        hostFilePath = QDir(workspace()).filePath(hostFilePath);
    }

    QFileInfo fi(hostFilePath);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QFile file(hostFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), QStringLiteral("无法写入目标文件:\n%1\n请检查文件是否被占用或目录权限。").arg(hostFilePath));
        return;
    }

    QTextStream out(&file);
    out << ui->inventoryContentEdit->toPlainText();
    file.close();

    appendLog(QStringLiteral("\n[配置提示] 已成功保存修改至主机清单: %1\n").arg(hostFilePath));
    QMessageBox::information(this, QStringLiteral("保存成功"), QStringLiteral("主机清单内容已成功保存！\n已自动刷新主机组列表。"));
    updateInventoryGroups();
}

void MainWindow::copyInventoryPreview()
{
    const QString text = ui->inventoryContentEdit->toPlainText();
    if (text.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前主机清单内容为空。"));
        return;
    }
    QApplication::clipboard()->setText(text);
    QMessageBox::information(this, QStringLiteral("已复制"), QStringLiteral("当前主机清单全部文本内容已复制到剪贴板。"));
}

void MainWindow::openPlaybookPreviewTab()
{
    loadPlaybookPreview();
    ui->logTabWidget->setCurrentWidget(ui->tabPlaybookPreview);
}

void MainWindow::loadPlaybookPreview()
{
    const QString currentPb = ui->playbookCombo->currentText().trimmed();
    if (currentPb.isEmpty()) {
        ui->labelPbPreviewFile->setText(QStringLiteral("当前剧本: (未选择 Playbook 剧本文件)"));
        ui->playbookContentEdit->setPlaceholderText(QStringLiteral("请在上方「批量部署」中指定或选择一个 Playbook 剧本文件 (*.yaml / *.yml)"));
        ui->playbookContentEdit->clear();
        return;
    }

    QString pbFilePath = currentPb;
    if (!QDir::isAbsolutePath(pbFilePath)) {
        pbFilePath = QDir(workspace()).filePath(pbFilePath);
    }

    QFile file(pbFilePath);
    if (!file.exists()) {
        ui->labelPbPreviewFile->setText(QStringLiteral("当前剧本: %1 (文件不存在)").arg(pbFilePath));
        ui->playbookContentEdit->clear();
        ui->playbookContentEdit->setPlaceholderText(QStringLiteral("剧本文件不存在: %1\n您可以直接在此处编写 YAML 剧本内容并点击「保存修改」快速新建。").arg(pbFilePath));
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->labelPbPreviewFile->setText(QStringLiteral("当前剧本: %1 (无法读取，权限不足)").arg(pbFilePath));
        return;
    }

    QTextStream in(&file);
    const QString content = in.readAll();
    file.close();

    const int lineCount = content.count(QLatin1Char('\n')) + (content.isEmpty() ? 0 : 1);
    ui->labelPbPreviewFile->setText(QStringLiteral("当前剧本: %1 (共 %2 行, 大小 %3 字节)").arg(pbFilePath).arg(lineCount).arg(content.toUtf8().size()));
    ui->playbookContentEdit->setPlainText(content);
}

void MainWindow::savePlaybookPreview()
{
    const QString currentPb = ui->playbookCombo->currentText().trimmed();
    if (currentPb.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先在「批量部署」中指定有效的 Playbook 剧本文件路径。"));
        return;
    }

    QString pbFilePath = currentPb;
    if (!QDir::isAbsolutePath(pbFilePath)) {
        pbFilePath = QDir(workspace()).filePath(pbFilePath);
    }

    QFileInfo fi(pbFilePath);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QFile file(pbFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), QStringLiteral("无法写入目标剧本文件:\n%1\n请检查文件是否被占用或目录权限。").arg(pbFilePath));
        return;
    }

    QTextStream out(&file);
    out << ui->playbookContentEdit->toPlainText();
    file.close();

    appendLog(QStringLiteral("\n[配置提示] 已成功保存修改至 Playbook 剧本: %1\n").arg(pbFilePath));
    QMessageBox::information(this, QStringLiteral("保存成功"), QStringLiteral("Playbook 剧本内容已成功保存！"));
}

void MainWindow::copyPlaybookPreview()
{
    const QString text = ui->playbookContentEdit->toPlainText();
    if (text.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前剧本内容为空。"));
        return;
    }
    QApplication::clipboard()->setText(text);
    QMessageBox::information(this, QStringLiteral("已复制"), QStringLiteral("当前 Playbook 剧本全部 YAML 内容已复制到剪贴板。"));
}
