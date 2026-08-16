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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , process(new QProcess(this))
    , externalLogTimer(new QTimer(this))
{
    ui->setupUi(this);

    // 设置执行历史表格列宽
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
    ui->workspaceEdit->setText(QDir::currentPath());
    scanFiles();
    updateInventoryGroups();
    updatePreview();

    // 定时轮询外部终端输出
    connect(externalLogTimer, &QTimer::timeout, this, &MainWindow::pollExternalTerminalOutput);
    externalLogTimer->start(400);

    // 进程标准输出和错误输出捕获
    connect(process, &QProcess::readyReadStandardOutput, this, [this]() {
        appendLog(QString::fromUtf8(process->readAllStandardOutput()));
    });
    connect(process, &QProcess::readyReadStandardError, this, [this]() {
        appendLog(QString::fromUtf8(process->readAllStandardError()));
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        appendLog(QStringLiteral("\n[进程已结束] 退出代码: %1\n").arg(exitCode));
        if (ui->stopButton) ui->stopButton->setEnabled(false);
    });

    // ================= 信号连接绑定 =================

    // 工作空间与扫描
    connect(ui->browseWorkspaceBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择工作空间目录"), workspace());
        if (!dir.isEmpty()) {
            ui->workspaceEdit->setText(dir);
            scanFiles();
            updateInventoryGroups();
            updatePreview();
        }
    });
    connect(ui->scanBtn, &QPushButton::clicked, this, [this]() {
        scanFiles();
        updateInventoryGroups();
        updatePreview();
    });
    connect(ui->workspaceEdit, &QLineEdit::textChanged, this, [this]() {
        scanFiles();
        updateInventoryGroups();
        updatePreview();
    });

    connect(ui->inventoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onInventorySelected);
    connect(ui->groupCombo, &QComboBox::currentTextChanged, this, &MainWindow::updatePreview);
    connect(ui->limitEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->extraArgsEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->playbookCombo, &QComboBox::currentTextChanged, this, &MainWindow::updatePreview);
    connect(ui->moduleCombo, &QComboBox::currentTextChanged, this, &MainWindow::updatePreview);
    connect(ui->commandEdit, &QPlainTextEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->localPathEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(ui->remotePathEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);

    // 选项卡切换联动更新预览
    connect(ui->actionTabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    // 常用快捷运维
    connect(ui->pingBtn, &QPushButton::clicked, this, [this]() {
        runProcess(QStringLiteral("ansible"), QStringList() << group() << commonArgs() << QStringLiteral("-m") << QStringLiteral("ping"), workspace());
    });
    connect(ui->factsBtn, &QPushButton::clicked, this, [this]() {
        runProcess(QStringLiteral("ansible"), QStringList() << group() << commonArgs() << QStringLiteral("-m") << QStringLiteral("setup"), workspace());
    });
    connect(ui->uptimeBtn, &QPushButton::clicked, this, [this]() {
        runProcess(QStringLiteral("ansible"), QStringList() << group() << commonArgs() << QStringLiteral("-m") << QStringLiteral("command") << QStringLiteral("-a") << QStringLiteral("uptime"), workspace());
    });
    connect(ui->diskBtn, &QPushButton::clicked, this, [this]() {
        runProcess(QStringLiteral("ansible"), QStringList() << group() << commonArgs() << QStringLiteral("-m") << QStringLiteral("command") << QStringLiteral("-a") << QStringLiteral("df -h"), workspace());
    });
    connect(ui->memoryBtn, &QPushButton::clicked, this, [this]() {
        runProcess(QStringLiteral("ansible"), QStringList() << group() << commonArgs() << QStringLiteral("-m") << QStringLiteral("command") << QStringLiteral("-a") << QStringLiteral("free -m"), workspace());
    });
    connect(ui->checkYamlBtn, &QPushButton::clicked, this, [this]() {
        const QString pb = ui->playbookCombo->currentText().trimmed();
        if (pb.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个待检查语法的 Playbook 文件。"));
            return;
        }
        runProcess(QStringLiteral("ansible-playbook"), QStringList() << pb << commonArgs() << QStringLiteral("--syntax-check"), workspace());
    });

    // Playbook 执行
    connect(ui->runPlaybookTermBtn, &QPushButton::clicked, this, [this]() {
        const QString pb = ui->playbookCombo->currentText().trimmed();
        if (pb.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择有效的 Playbook 剧本文件。"));
            return;
        }
        runInExternalTerminal(QStringLiteral("ansible-playbook"), QStringList() << pb << commonArgs(), workspace());
    });
    connect(ui->runPlaybookBgBtn, &QPushButton::clicked, this, [this]() {
        const QString pb = ui->playbookCombo->currentText().trimmed();
        if (pb.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择有效的 Playbook 剧本文件。"));
            return;
        }
        runProcess(QStringLiteral("ansible-playbook"), QStringList() << pb << commonArgs(), workspace());
    });

    // 命令执行
    connect(ui->runCmdTermBtn, &QPushButton::clicked, this, [this]() {
        const QString cmd = ui->commandEdit->toPlainText().trimmed();
        if (cmd.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入要执行的远程命令内容。"));
            return;
        }
        runInExternalTerminal(QStringLiteral("ansible"), QStringList() << group() << commonArgs() << QStringLiteral("-m") << ui->moduleCombo->currentText() << QStringLiteral("-a") << cmd, workspace());
    });
    connect(ui->runCmdBgBtn, &QPushButton::clicked, this, [this]() {
        const QString cmd = ui->commandEdit->toPlainText().trimmed();
        if (cmd.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入要执行的远程命令内容。"));
            return;
        }
        runProcess(QStringLiteral("ansible"), QStringList() << group() << commonArgs() << QStringLiteral("-m") << ui->moduleCombo->currentText() << QStringLiteral("-a") << cmd, workspace());
    });

    // 文件分发
    connect(ui->browseLocalFileBtn, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("选择本地待分发文件"), workspace());
        if (!file.isEmpty()) ui->localPathEdit->setText(file);
    });
    connect(ui->runCopyTermBtn, &QPushButton::clicked, this, [this]() {
        const QString src = ui->localPathEdit->text().trimmed();
        const QString dest = ui->remotePathEdit->text().trimmed();
        if (src.isEmpty() || dest.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请完整填写本地源路径和远程目标路径。"));
            return;
        }
        const QString copyArgs = QStringLiteral("src=%1 dest=%2").arg(src, dest);
        runInExternalTerminal(QStringLiteral("ansible"), QStringList() << group() << commonArgs() << QStringLiteral("-m") << QStringLiteral("copy") << QStringLiteral("-a") << copyArgs, workspace());
    });

    // 复制指令预览
    connect(ui->copyPreviewBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(ui->previewEdit->toPlainText());
        QMessageBox::information(this, QStringLiteral("已复制"), QStringLiteral("完整 Ansible 指令已复制到系统剪贴板。"));
    });

    // 日志操作
    connect(ui->clearLogBtn, &QPushButton::clicked, this, &MainWindow::clearLogs);
    connect(ui->copyLogBtn, &QPushButton::clicked, this, &MainWindow::copyLogs);
    connect(ui->exportLogBtn, &QPushButton::clicked, this, &MainWindow::exportLogs);
    connect(ui->searchBtn, &QPushButton::clicked, this, [this]() {
        onSearchLog(ui->searchLogEdit->text().trimmed());
    });
    connect(ui->searchLogEdit, &QLineEdit::returnPressed, this, [this]() {
        onSearchLog(ui->searchLogEdit->text().trimmed());
    });
    connect(ui->stopButton, &QPushButton::clicked, this, [this]() {
        if (process && process->state() != QProcess::NotRunning) {
            process->terminate();
            QTimer::singleShot(2000, this, [this]() {
                if (process && process->state() != QProcess::NotRunning) {
                    process->kill();
                }
            });
            appendLog(QStringLiteral("\n[已发出终止信号] 正在停止任务...\n"));
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onTabChanged(int)
{
    updatePreview();
}

void MainWindow::onInventorySelected(int)
{
    updateInventoryGroups();
    updatePreview();
}

void MainWindow::clearLogs()
{
    ui->logEdit->clear();
}

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

// 智能解析 INI hosts 文件中的分组名 [group_name]
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

    QString currentGroup = ui->groupCombo->currentText();
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

    ui->groupCombo->blockSignals(true);
    ui->groupCombo->clear();
    ui->groupCombo->addItems(groups);
    int idx = ui->groupCombo->findText(currentGroup);
    if (idx >= 0) {
        ui->groupCombo->setCurrentIndex(idx);
    } else if (ui->groupCombo->count() > 0) {
        ui->groupCombo->setCurrentIndex(0);
    }
    ui->groupCombo->blockSignals(false);
}

void MainWindow::scanFiles()
{
    const QDir dir(workspace());
    if (!dir.exists()) return;

    const QString oldInv = ui->inventoryCombo->currentText();
    const QString oldPb = ui->playbookCombo->currentText();

    ui->inventoryCombo->clear();
    ui->playbookCombo->clear();

    const QStringList inventoryCandidates = {
        QStringLiteral("hosts"),
        QStringLiteral("inventory"),
        QStringLiteral("inventory.ini"),
        QStringLiteral("inventory.yaml"),
        QStringLiteral("inventory.yml"),
        QStringLiteral("my_hosts")
    };

    for (const QString &name : inventoryCandidates) {
        if (dir.exists(name)) ui->inventoryCombo->addItem(name);
    }

    const QStringList pbEntries = dir.entryList(
        QStringList() << QStringLiteral("*.yaml") << QStringLiteral("*.yml"),
        QDir::Files,
        QDir::Name
    );
    for (const QString &name : pbEntries) {
        ui->playbookCombo->addItem(name);
    }

    if (ui->inventoryCombo->count() == 0) ui->inventoryCombo->addItem(QStringLiteral("my_hosts"));
    if (ui->playbookCombo->count() == 0) ui->playbookCombo->addItem(QStringLiteral("ansible.yaml"));

    int invIdx = ui->inventoryCombo->findText(oldInv);
    if (invIdx >= 0) ui->inventoryCombo->setCurrentIndex(invIdx);

    int pbIdx = ui->playbookCombo->findText(oldPb);
    if (pbIdx >= 0) ui->playbookCombo->setCurrentIndex(pbIdx);
}

// ANSI 颜色转义字符转换为 HTML 富文本 (适配 Linux 终端色彩)
QString MainWindow::ansiToHtml(const QString &text) const
{
    QString html = text.toHtmlEscaped();

    html.replace(QRegularExpression(QStringLiteral("\033\\[0?32m")), QStringLiteral("<span style=\"color:#22c55e;font-weight:bold;\">")); // 绿
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?31m")), QStringLiteral("<span style=\"color:#ef4444;font-weight:bold;\">")); // 红
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?33m")), QStringLiteral("<span style=\"color:#f59e0b;font-weight:bold;\">")); // 黄
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?36m")), QStringLiteral("<span style=\"color:#06b6d4;font-weight:bold;\">")); // 青
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?34m")), QStringLiteral("<span style=\"color:#3b82f6;font-weight:bold;\">")); // 蓝
    html.replace(QRegularExpression(QStringLiteral("\033\\[0?35m")), QStringLiteral("<span style=\"color:#a855f7;font-weight:bold;\">")); // 紫
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
    const int currentTab = ui->actionTabs->currentIndex();

    if (currentTab == 0) {
        preview = QStringLiteral("ansible %1 %2 -m ping").arg(group(), commonArgs().join(QLatin1Char(' ')));
    } else if (currentTab == 1) {
        const QString pb = ui->playbookCombo->currentText().trimmed();
        preview = QStringLiteral("ansible-playbook %1 %2").arg(pb, commonArgs().join(QLatin1Char(' ')));
    } else if (currentTab == 2) {
        const QString cmd = ui->commandEdit->toPlainText().trimmed();
        const QString cmdArg = cmd.isEmpty() ? QStringLiteral("<command>") : shellQuote(cmd);
        preview = QStringLiteral("ansible %1 %2 -m %3 -a %4")
                      .arg(group(), commonArgs().join(QLatin1Char(' ')), ui->moduleCombo->currentText(), cmdArg);
    } else if (currentTab == 3) {
        const QString src = ui->localPathEdit->text().trimmed();
        const QString dest = ui->remotePathEdit->text().trimmed();
        preview = QStringLiteral("ansible %1 %2 -m copy -a %3")
                      .arg(group(), commonArgs().join(QLatin1Char(' ')), shellQuote(QStringLiteral("src=%1 dest=%2").arg(src, dest)));
    }

    ui->previewEdit->setPlainText(preview.simplified());
}

QString MainWindow::workspace() const { return ui->workspaceEdit->text().trimmed(); }
QString MainWindow::inventory() const { return ui->inventoryCombo->currentText().trimmed(); }
QString MainWindow::group() const { return ui->groupCombo->currentText().trimmed(); }
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

bool MainWindow::ensureSharedMemoryAvailable()
{
    QFile shm(QStringLiteral("/dev/shm"));
    return shm.exists();
}

int MainWindow::recordHistoryStart(const QString &command)
{
    const int row = ui->historyTable->rowCount();
    ui->historyTable->insertRow(row);

    const QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    ui->historyTable->setItem(row, 0, new QTableWidgetItem(timeStr));
    ui->historyTable->setItem(row, 1, new QTableWidgetItem(command));

    auto *statusItem = new QTableWidgetItem(QStringLiteral("运行中"));
    statusItem->setForeground(QBrush(QColor(59, 130, 246)));
    ui->historyTable->setItem(row, 2, statusItem);

    ui->historyTable->setItem(row, 3, new QTableWidgetItem(QStringLiteral("-")));
    ui->historyTable->setItem(row, 4, new QTableWidgetItem(QStringLiteral("计时中...")));
    ui->historyTable->setItem(row, 5, new QTableWidgetItem(QStringLiteral("进行中")));

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
            statusItem->setForeground(QBrush(QColor(34, 197, 94)));
        } else {
            statusItem->setForeground(QBrush(QColor(239, 68, 68)));
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
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("当前有正在运行的内置任务，请等待其完成或点击终止。"));
        return;
    }

    const QString fullCommand = program + QLatin1Char(' ') + arguments.join(QLatin1Char(' '));
    const int historyRow = recordHistoryStart(fullCommand);
    const qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    appendLog(QStringLiteral("\n------------------------------------------------------------\n"));
    appendLog(QStringLiteral("[启动内置任务] 工作空间: %1\n").arg(workingDirectory));
    appendLog(QStringLiteral("[执行完整指令] %1\n").arg(fullCommand));
    appendLog(QStringLiteral("------------------------------------------------------------\n"));

    if (ui->stopButton) ui->stopButton->setEnabled(true);

    process->setWorkingDirectory(workingDirectory);
    
    disconnect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), nullptr, nullptr);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, historyRow, startTime](int exitCode, QProcess::ExitStatus) {
        qint64 durationMs = QDateTime::currentMSecsSinceEpoch() - startTime;
        appendLog(QStringLiteral("\n[内置任务结束] 退出代码: %1 (耗时 %2 秒)\n").arg(exitCode).arg(durationMs / 1000.0, 0, 'f', 2));
        recordHistoryFinish(historyRow, exitCode == 0 ? QStringLiteral("成功") : QStringLiteral("失败"), QString::number(exitCode), durationMs);
        if (ui->stopButton) ui->stopButton->setEnabled(false);
    });

    process->start(program, arguments);
}

void MainWindow::runInExternalTerminal(const QString &program, const QStringList &arguments, const QString &workingDirectory)
{
    ensureSharedMemoryAvailable();

    const QString token = QString::number(QDateTime::currentMSecsSinceEpoch());
    const QString scriptPath = QDir::temp().filePath(QStringLiteral("ansible-pilot-%1.sh").arg(token));
    const QString logPath = QDir::temp().filePath(QStringLiteral("ansible-pilot-%1.log").arg(token));
    const QString statusPath = QDir::temp().filePath(QStringLiteral("ansible-pilot-%1.status").arg(token));
    const QString pidPath = QDir::temp().filePath(QStringLiteral("ansible-pilot-%1.pid").arg(token));

    QString fullCmd = program;
    for (const QString &arg : arguments) {
        fullCmd += QLatin1Char(' ') + shellQuote(arg);
    }

    const int historyRow = recordHistoryStart(fullCmd + QStringLiteral(" [外部终端]"));

    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog(QStringLiteral("[错误] 无法创建临时执行脚本: %1\n").arg(scriptPath));
        recordHistoryFinish(historyRow, QStringLiteral("失败"), QStringLiteral("-1"));
        return;
    }

    QTextStream out(&scriptFile);
    out << "#!/usr/bin/env bash\n";
    out << "cd " << shellQuote(workingDirectory) << " || exit 1\n";
    out << "echo $$ > " << shellQuote(pidPath) << "\n";
    out << ": > " << shellQuote(logPath) << "\n";
    out << "echo '============================================================'\n";
    out << "echo ' AnsiblePilot 自动化运维任务执行终端'\n";
    out << "echo ' 执行指令: " << fullCmd.replace(QLatin1Char('\''), QStringLiteral("\\'")) << "'\n";
    out << "echo '============================================================'\n\n";
    out << "export ANSIBLE_FORCE_COLOR=true\n";
    out << "export PYTHONUNBUFFERED=1\n";
    out << fullCmd << " 2>&1 | tee -a " << shellQuote(logPath) << "\n";
    out << "EC=${PIPESTATUS[0]}\n";
    out << "echo $EC > " << shellQuote(statusPath) << "\n";
    out << "\necho ''\n";
    out << "echo '============================================================'\n";
    out << "echo \"任务已执行完毕，退出代码: $EC\"\n";
    out << "echo '按 Enter 回车键关闭当前终端窗口...'\n";
    out << "echo '============================================================'\n";
    out << "read -r\n";
    scriptFile.close();

    QFile::setPermissions(scriptPath,
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                          QFile::ReadGroup | QFile::ExeGroup |
                          QFile::ReadOther | QFile::ExeOther);

    appendLog(QStringLiteral("\n------------------------------------------------------------\n"));
    appendLog(QStringLiteral("[拉起外部独立终端] 执行指令: %1\n").arg(fullCmd));
    appendLog(QStringLiteral("[日志同步流] %1\n").arg(logPath));
    appendLog(QStringLiteral("------------------------------------------------------------\n"));

    ExternalRun run;
    run.logPath = logPath;
    run.statusPath = statusPath;
    run.pidPath = pidPath;
    run.logOffset = 0;
    run.historyRow = historyRow;
    run.startTime = QDateTime::currentDateTime();
    activeRuns.append(run);

    if (!QProcess::startDetached(QStringLiteral("x-terminal-emulator"), QStringList() << QStringLiteral("-e") << scriptPath)) {
        if (!QProcess::startDetached(QStringLiteral("gnome-terminal"), QStringList() << QStringLiteral("--") << scriptPath)) {
            if (!QProcess::startDetached(QStringLiteral("mate-terminal"), QStringList() << QStringLiteral("-e") << scriptPath)) {
                if (!QProcess::startDetached(QStringLiteral("kylin-terminal"), QStringList() << QStringLiteral("-e") << scriptPath)) {
                    if (!QProcess::startDetached(QStringLiteral("konsole"), QStringList() << QStringLiteral("-e") << scriptPath)) {
                        if (!QProcess::startDetached(QStringLiteral("xterm"), QStringList() << QStringLiteral("-e") << scriptPath)) {
                            appendLog(QStringLiteral("[提示] 未检测到图形终端模拟器，已自动切换至后台捕获模式执行。\n"));
                            runProcess(QStringLiteral("bash"), QStringList() << scriptPath, workingDirectory);
                        }
                    }
                }
            }
        }
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
            appendLog(QStringLiteral("\n[外部终端任务已完成] 退出代码: %1 (耗时 %2 秒)\n").arg(exitCode).arg(durationMs / 1000.0, 0, 'f', 2));
            recordHistoryFinish(run.historyRow, exitCode == QStringLiteral("0") ? QStringLiteral("成功") : QStringLiteral("失败"), exitCode, durationMs);

            activeRuns.removeAt(i);
        }
    }
}
