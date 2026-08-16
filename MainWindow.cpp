#include "MainWindow.h"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QScrollBar>
#include <QDirIterator>
#include <QGridLayout>
#include <QGroupBox>
#include <QIODevice>
#include <QLabel>
#include <QList>
#include <QLineEdit>
#include <QPair>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QStandardPaths>
#include <QSizePolicy>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QColor>
#include <QHBoxLayout>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

static QString cleanPath(const QString &path)
{
    return QDir::toNativeSeparators(QDir::cleanPath(path));
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), process(new QProcess(this))
{
    externalLogTimer = new QTimer(this);
    externalLogTimer->setInterval(500);
    connect(externalLogTimer, &QTimer::timeout, this, &MainWindow::pollExternalTerminalOutput);

    buildUi();
    scanFiles();
    updatePreview();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Ansible 批量运维工具"));
    resize(1280, 820);
    setMinimumSize(1040, 720);
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f5f7fb; }"
        "QGroupBox {"
        "  background: #ffffff;"
        "  border: 1px solid #d9e2ef;"
        "  border-radius: 10px;"
        "  margin-top: 12px;"
        "  padding: 14px 12px 12px 12px;"
        "  font-weight: 600;"
        "  color: #24364b;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 14px;"
        "  padding: 0 8px;"
        "}"
        "QLabel { color: #334155; }"
        "QLineEdit, QComboBox, QPlainTextEdit {"
        "  background: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 6px 8px;"
        "  selection-background-color: #2563eb;"
        "}"
        "QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus {"
        "  border: 1px solid #2563eb;"
        "}"
        "QPushButton {"
        "  background: #eef2ff;"
        "  color: #1e3a8a;"
        "  border: 1px solid #c7d2fe;"
        "  border-radius: 6px;"
        "  padding: 7px 14px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: #e0e7ff; }"
        "QPushButton:pressed { background: #c7d2fe; }"
        "QPushButton#primaryButton {"
        "  background: #2563eb;"
        "  color: white;"
        "  border: 1px solid #1d4ed8;"
        "}"
        "QPushButton#primaryButton:hover { background: #1d4ed8; }"
        "QPushButton#dangerButton {"
        "  background: #fee2e2;"
        "  color: #991b1b;"
        "  border: 1px solid #fecaca;"
        "}"
        "QPushButton#dangerButton:hover { background: #fecaca; }"
        "QPushButton:disabled {"
        "  background: #e5e7eb;"
        "  color: #9ca3af;"
        "  border: 1px solid #d1d5db;"
        "}"
        "QTabWidget::pane {"
        "  border: 1px solid #d9e2ef;"
        "  border-radius: 10px;"
        "  background: #ffffff;"
        "  top: -1px;"
        "}"
        "QTabBar::tab {"
        "  background: #e2e8f0;"
        "  color: #334155;"
        "  padding: 9px 22px;"
        "  border-top-left-radius: 8px;"
        "  border-top-right-radius: 8px;"
        "  margin-right: 4px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: #ffffff;"
        "  color: #1d4ed8;"
        "  font-weight: 700;"
        "}"
    ));

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(18, 14, 18, 14);
    root->setSpacing(12);
    auto *settings = new QGroupBox(QStringLiteral("基础配置"), central);
    auto *grid = new QGridLayout(settings);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 3);
    grid->setColumnStretch(2, 2);
    grid->setColumnStretch(3, 2);

    workspaceEdit = new QLineEdit(settings);
#ifdef ANSIBLE_WORKSPACE_PATH
    workspaceEdit->setText(cleanPath(QString::fromUtf8(ANSIBLE_WORKSPACE_PATH)));
#else
    workspaceEdit->setText(cleanPath(QDir::currentPath()));
#endif
    inventoryCombo = new QComboBox(settings);
    inventoryCombo->setEditable(true);
    groupCombo = new QComboBox(settings);
    groupCombo->setEditable(true);
    groupCombo->addItem(QStringLiteral("web"));
    limitEdit = new QLineEdit(settings);
    limitEdit->setPlaceholderText(QStringLiteral("可选：限制主机，例如 172.10.10.11"));
    extraArgsEdit = new QLineEdit(settings);
    extraArgsEdit->setPlaceholderText(QStringLiteral("可选：--ask-pass --ask-become-pass --check"));

    auto *workspaceButton = new QPushButton(QStringLiteral("选择目录"), settings);
    auto *inventoryButton = new QPushButton(QStringLiteral("选择 Inventory"), settings);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新扫描"), settings);
    workspaceButton->setMinimumWidth(112);
    inventoryButton->setMinimumWidth(112);
    refreshButton->setMinimumWidth(112);

    grid->addWidget(new QLabel(QStringLiteral("工作目录"), settings), 0, 0);
    grid->addWidget(workspaceEdit, 0, 1, 1, 4);
    grid->addWidget(workspaceButton, 0, 5);
    grid->addWidget(new QLabel(QStringLiteral("Inventory"), settings), 1, 0);
    grid->addWidget(inventoryCombo, 1, 1, 1, 2);
    grid->addWidget(inventoryButton, 1, 3);
    grid->addWidget(new QLabel(QStringLiteral("主机组"), settings), 1, 4);
    grid->addWidget(groupCombo, 1, 5);
    grid->addWidget(new QLabel(QStringLiteral("Limit"), settings), 2, 0);
    grid->addWidget(limitEdit, 2, 1, 1, 3);
    grid->addWidget(refreshButton, 2, 4, 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("额外参数"), settings), 3, 0);
    grid->addWidget(extraArgsEdit, 3, 1, 1, 5);

    auto *tabs = new QTabWidget(central);
    tabs->setDocumentMode(true);
    tabs->setMinimumHeight(320);
    auto *playbookTab = new QWidget(tabs);
    auto *playbookGrid = new QGridLayout(playbookTab);
    playbookGrid->setContentsMargins(18, 18, 18, 18);
    playbookGrid->setHorizontalSpacing(12);
    playbookGrid->setVerticalSpacing(12);
    playbookGrid->setColumnStretch(1, 1);
    playbookCombo = new QComboBox(playbookTab);
    playbookCombo->setEditable(true);
    auto *playbookButton = new QPushButton(QStringLiteral("选择 Playbook"), playbookTab);
    auto *pingButton = new QPushButton(QStringLiteral("Ping 测试"), playbookTab);
    auto *runPlaybookButton = new QPushButton(QStringLiteral("执行选中 Playbook"), playbookTab);
    pingButton->setObjectName(QStringLiteral("primaryButton"));
    runPlaybookButton->setObjectName(QStringLiteral("primaryButton"));
    playbookButton->setMinimumWidth(128);
    pingButton->setMinimumWidth(128);
    runPlaybookButton->setMinimumWidth(160);
    playbookGrid->addWidget(new QLabel(QStringLiteral("Playbook"), playbookTab), 0, 0);
    playbookGrid->addWidget(playbookCombo, 0, 1, 1, 3);
    playbookGrid->addWidget(playbookButton, 0, 4);
    playbookGrid->addWidget(pingButton, 1, 3);
    playbookGrid->addWidget(runPlaybookButton, 1, 4);
    playbookGrid->setRowStretch(2, 1);

    auto *commandTab = new QWidget(tabs);
    auto *commandGrid = new QGridLayout(commandTab);
    commandGrid->setContentsMargins(18, 18, 18, 18);
    commandGrid->setHorizontalSpacing(12);
    commandGrid->setVerticalSpacing(12);
    commandGrid->setColumnStretch(1, 1);
    moduleCombo = new QComboBox(commandTab);
    moduleCombo->setEditable(true);
    moduleCombo->addItems({QStringLiteral("shell"), QStringLiteral("command"), QStringLiteral("service"), QStringLiteral("systemd")});
    commandEdit = new QPlainTextEdit(commandTab);
    commandEdit->setPlaceholderText(QStringLiteral("例如：ps -ef | grep serveragent"));
    commandEdit->setMinimumHeight(96);
    auto *runCommandButton = new QPushButton(QStringLiteral("批量执行命令"), commandTab);
    runCommandButton->setObjectName(QStringLiteral("primaryButton"));
    runCommandButton->setMinimumWidth(150);
    commandGrid->addWidget(new QLabel(QStringLiteral("模块"), commandTab), 0, 0);
    commandGrid->addWidget(moduleCombo, 0, 1);
    commandGrid->addWidget(new QLabel(QStringLiteral("参数"), commandTab), 1, 0);
    commandGrid->addWidget(commandEdit, 1, 1, 1, 4);
    commandGrid->addWidget(runCommandButton, 2, 4);

    auto *copyTab = new QWidget(tabs);
    auto *copyGrid = new QGridLayout(copyTab);
    copyGrid->setContentsMargins(18, 18, 18, 18);
    copyGrid->setHorizontalSpacing(12);
    copyGrid->setVerticalSpacing(12);
    copyGrid->setColumnStretch(1, 1);
    localPathEdit = new QLineEdit(copyTab);
    remotePathEdit = new QLineEdit(QStringLiteral("/root/work/missionschedulingsoftware/"), copyTab);
    auto *localButton = new QPushButton(QStringLiteral("选择本地文件/目录"), copyTab);
    auto *runCopyButton = new QPushButton(QStringLiteral("批量上传"), copyTab);
    localButton->setMinimumWidth(150);
    runCopyButton->setObjectName(QStringLiteral("primaryButton"));
    runCopyButton->setMinimumWidth(120);
    copyGrid->addWidget(new QLabel(QStringLiteral("本地路径"), copyTab), 0, 0);
    copyGrid->addWidget(localPathEdit, 0, 1, 1, 3);
    copyGrid->addWidget(localButton, 0, 4);
    copyGrid->addWidget(new QLabel(QStringLiteral("远端路径"), copyTab), 1, 0);
    copyGrid->addWidget(remotePathEdit, 1, 1, 1, 3);
    copyGrid->addWidget(runCopyButton, 1, 4);

    auto runShell = [this](const QString &cmd) {
        QStringList a{QStringLiteral("-i"), inventory(), group(), QStringLiteral("-m"), QStringLiteral("shell"), QStringLiteral("-a"), cmd};
        a += commonArgs();
        runInExternalTerminal(QStringLiteral("ansible"), a, workspace());
    };
    auto runAdHoc = [this](const QString &module, const QString &moduleArgs) {
        QStringList a{QStringLiteral("-i"), inventory(), group(), QStringLiteral("-m"), module, QStringLiteral("-a"), moduleArgs};
        a += commonArgs();
        runInExternalTerminal(QStringLiteral("ansible"), a, workspace());
    };

    auto *commonTab = new QWidget(tabs);
    auto *commonOuter = new QVBoxLayout(commonTab);
    commonOuter->setContentsMargins(12, 10, 12, 10);
    commonOuter->setSpacing(8);
    auto *commonHint = new QLabel(QStringLiteral(
        "下面是常用运维操作。点击按钮即可一键执行，无需自己写 playbook；"
        "命令会以原始 Ansible 形式同步到下方日志区域，方便对照学习。"), commonTab);
    commonHint->setWordWrap(true);
    commonHint->setStyleSheet(QStringLiteral("color:#475569;"));
    commonOuter->addWidget(commonHint);
    auto *commonSubTabs = new QTabWidget(commonTab);
    commonSubTabs->setDocumentMode(true);
    commonOuter->addWidget(commonSubTabs, 1);

    // ---------- 服务管理 ----------
    auto *serviceTab = new QWidget(commonSubTabs);
    auto *serviceGrid = new QGridLayout(serviceTab);
    serviceGrid->setContentsMargins(16, 16, 16, 16);
    serviceGrid->setHorizontalSpacing(10);
    serviceGrid->setVerticalSpacing(10);
    for (int c = 0; c < 4; ++c) serviceGrid->setColumnStretch(c, 1);
    auto *serviceNameEdit = new QLineEdit(serviceTab);
    serviceNameEdit->setPlaceholderText(QStringLiteral("例如：sshd、nginx"));
    auto *svcStartBtn = new QPushButton(QStringLiteral("启动服务"), serviceTab);
    auto *svcStopBtn = new QPushButton(QStringLiteral("停止服务"), serviceTab);
    auto *svcRestartBtn = new QPushButton(QStringLiteral("重启服务"), serviceTab);
    auto *svcStatusBtn = new QPushButton(QStringLiteral("查看状态"), serviceTab);
    svcRestartBtn->setObjectName(QStringLiteral("primaryButton"));
    serviceGrid->addWidget(new QLabel(QStringLiteral("服务名"), serviceTab), 0, 0);
    serviceGrid->addWidget(serviceNameEdit, 0, 1, 1, 4);
    serviceGrid->addWidget(svcStartBtn, 1, 0);
    serviceGrid->addWidget(svcStopBtn, 1, 1);
    serviceGrid->addWidget(svcRestartBtn, 1, 2);
    serviceGrid->addWidget(svcStatusBtn, 1, 3);
    serviceGrid->setRowStretch(2, 1);
    auto runService = [this, serviceNameEdit, runAdHoc, runShell](const QString &state) {
        const QString name = serviceNameEdit->text().trimmed();
        if (name.isEmpty()) { appendLog(QStringLiteral("\n请先填写服务名。\n")); return; }
        if (state == QStringLiteral("status")) {
            runShell(QStringLiteral("systemctl status %1 --no-pager").arg(name));
        } else {
            runAdHoc(QStringLiteral("systemd"), QStringLiteral("name=%1 state=%2").arg(name, state));
        }
    };
    connect(svcStartBtn, &QPushButton::clicked, this, [runService] { runService(QStringLiteral("started")); });
    connect(svcStopBtn, &QPushButton::clicked, this, [runService] { runService(QStringLiteral("stopped")); });
    connect(svcRestartBtn, &QPushButton::clicked, this, [runService] { runService(QStringLiteral("restarted")); });
    connect(svcStatusBtn, &QPushButton::clicked, this, [runService] { runService(QStringLiteral("status")); });
    commonSubTabs->addTab(serviceTab, QStringLiteral("服务管理"));

    // ---------- 系统信息 ----------
    auto *sysTab = new QWidget(commonSubTabs);
    auto *sysGrid = new QGridLayout(sysTab);
    sysGrid->setContentsMargins(16, 16, 16, 16);
    sysGrid->setHorizontalSpacing(10);
    sysGrid->setVerticalSpacing(10);
    sysGrid->setColumnStretch(0, 1);
    sysGrid->setColumnStretch(1, 1);
    auto *sysDiskBtn = new QPushButton(QStringLiteral("查看磁盘 (df -h)"), sysTab);
    auto *sysMemBtn = new QPushButton(QStringLiteral("查看内存 (free -h)"), sysTab);
    auto *sysUptimeBtn = new QPushButton(QStringLiteral("查看运行时间 (uptime)"), sysTab);
    auto *sysHostBtn = new QPushButton(QStringLiteral("查看主机名 / 内核"), sysTab);
    auto *sysCpuBtn = new QPushButton(QStringLiteral("查看 CPU 负载 (top -b -n1)"), sysTab);
    auto *sysWhoBtn = new QPushButton(QStringLiteral("查看当前登录用户 (who)"), sysTab);
    sysGrid->addWidget(sysDiskBtn, 0, 0);
    sysGrid->addWidget(sysMemBtn, 0, 1);
    sysGrid->addWidget(sysUptimeBtn, 1, 0);
    sysGrid->addWidget(sysHostBtn, 1, 1);
    sysGrid->addWidget(sysCpuBtn, 2, 0);
    sysGrid->addWidget(sysWhoBtn, 2, 1);
    sysGrid->setRowStretch(3, 1);
    connect(sysDiskBtn, &QPushButton::clicked, this, [runShell] { runShell(QStringLiteral("df -h")); });
    connect(sysMemBtn, &QPushButton::clicked, this, [runShell] { runShell(QStringLiteral("free -h")); });
    connect(sysUptimeBtn, &QPushButton::clicked, this, [runShell] { runShell(QStringLiteral("uptime")); });
    connect(sysHostBtn, &QPushButton::clicked, this, [runShell] { runShell(QStringLiteral("hostname && uname -a")); });
    connect(sysCpuBtn, &QPushButton::clicked, this, [runShell] { runShell(QStringLiteral("top -b -n1 | head -20")); });
    connect(sysWhoBtn, &QPushButton::clicked, this, [runShell] { runShell(QStringLiteral("who")); });
    commonSubTabs->addTab(sysTab, QStringLiteral("系统信息"));

    // ---------- 进程与日志 ----------
    auto *procTab = new QWidget(commonSubTabs);
    auto *procGrid = new QGridLayout(procTab);
    procGrid->setContentsMargins(16, 16, 16, 16);
    procGrid->setHorizontalSpacing(10);
    procGrid->setVerticalSpacing(10);
    procGrid->setColumnStretch(0, 0);
    for (int c = 1; c <= 3; ++c) procGrid->setColumnStretch(c, 1);
    auto *procNameEdit = new QLineEdit(procTab);
    procNameEdit->setPlaceholderText(QStringLiteral("进程名关键字，例如 nginx、serveragent"));
    auto *procPsBtn = new QPushButton(QStringLiteral("查看进程"), procTab);
    auto *procKillBtn = new QPushButton(QStringLiteral("杀进程 (pkill -f)"), procTab);
    procKillBtn->setObjectName(QStringLiteral("dangerButton"));
    auto *logPathEdit = new QLineEdit(procTab);
    logPathEdit->setPlaceholderText(QStringLiteral("日志路径，例如 /var/log/messages"));
    auto *logLinesEdit = new QLineEdit(QStringLiteral("200"), procTab);
    logLinesEdit->setMaximumWidth(80);
    auto *logTailBtn = new QPushButton(QStringLiteral("Tail 日志"), procTab);
    auto *logGrepEdit = new QLineEdit(procTab);
    logGrepEdit->setPlaceholderText(QStringLiteral("可选：在日志中过滤的关键字"));
    auto *logGrepBtn = new QPushButton(QStringLiteral("Grep 日志"), procTab);
    procGrid->addWidget(new QLabel(QStringLiteral("进程关键字"), procTab), 0, 0);
    procGrid->addWidget(procNameEdit, 0, 1, 1, 3);
    procGrid->addWidget(procPsBtn, 1, 1);
    procGrid->addWidget(procKillBtn, 1, 2);
    procGrid->addWidget(new QLabel(QStringLiteral("日志路径"), procTab), 2, 0);
    procGrid->addWidget(logPathEdit, 2, 1, 1, 3);
    procGrid->addWidget(new QLabel(QStringLiteral("行数"), procTab), 3, 0);
    procGrid->addWidget(logLinesEdit, 3, 1);
    procGrid->addWidget(logTailBtn, 3, 2);
    procGrid->addWidget(new QLabel(QStringLiteral("关键字"), procTab), 4, 0);
    procGrid->addWidget(logGrepEdit, 4, 1, 1, 2);
    procGrid->addWidget(logGrepBtn, 4, 3);
    procGrid->setRowStretch(5, 1);
    connect(procPsBtn, &QPushButton::clicked, this, [this, procNameEdit, runShell] {
        const QString k = procNameEdit->text().trimmed();
        if (k.isEmpty()) { appendLog(QStringLiteral("\n请先填写进程关键字。\n")); return; }
        runShell(QStringLiteral("ps -ef | grep -v grep | grep %1 || echo '无匹配进程'").arg(k));
    });
    connect(procKillBtn, &QPushButton::clicked, this, [this, procNameEdit, runShell] {
        const QString k = procNameEdit->text().trimmed();
        if (k.isEmpty()) { appendLog(QStringLiteral("\n请先填写进程关键字。\n")); return; }
        runShell(QStringLiteral("pkill -f %1; echo \"已对包含 %1 的进程发送 TERM\"").arg(k));
    });
    connect(logTailBtn, &QPushButton::clicked, this, [this, logPathEdit, logLinesEdit, runShell] {
        const QString p = logPathEdit->text().trimmed();
        if (p.isEmpty()) { appendLog(QStringLiteral("\n请先填写日志路径。\n")); return; }
        const QString n = logLinesEdit->text().trimmed().isEmpty() ? QStringLiteral("200") : logLinesEdit->text().trimmed();
        runShell(QStringLiteral("tail -n %1 %2").arg(n, p));
    });
    connect(logGrepBtn, &QPushButton::clicked, this, [this, logPathEdit, logGrepEdit, runShell] {
        const QString p = logPathEdit->text().trimmed();
        const QString k = logGrepEdit->text().trimmed();
        if (p.isEmpty() || k.isEmpty()) { appendLog(QStringLiteral("\n请同时填写日志路径和关键字。\n")); return; }
        runShell(QStringLiteral("grep -n -- %1 %2 | tail -n 200").arg(k, p));
    });
    commonSubTabs->addTab(procTab, QStringLiteral("进程与日志"));

    // ---------- 文件操作 ----------
    auto *fileTab = new QWidget(commonSubTabs);
    auto *fileGrid = new QGridLayout(fileTab);
    fileGrid->setContentsMargins(16, 16, 16, 16);
    fileGrid->setHorizontalSpacing(10);
    fileGrid->setVerticalSpacing(10);
    fileGrid->setColumnStretch(0, 0);
    for (int c = 1; c <= 4; ++c) fileGrid->setColumnStretch(c, 1);
    fileGrid->setColumnStretch(5, 0);
    auto *fileFetchSrc = new QLineEdit(fileTab);
    fileFetchSrc->setPlaceholderText(QStringLiteral("远端文件路径，例如 /var/log/messages"));
    auto *fileFetchDest = new QLineEdit(fileTab);
    fileFetchDest->setPlaceholderText(QStringLiteral("本地保存目录，例如 /tmp/fetched"));
    auto *fileFetchBtn = new QPushButton(QStringLiteral("从远端下载"), fileTab);
    auto *fileDeletePath = new QLineEdit(fileTab);
    fileDeletePath->setPlaceholderText(QStringLiteral("远端路径"));
    auto *fileDeleteBtn = new QPushButton(QStringLiteral("删除远端文件/目录"), fileTab);
    fileDeleteBtn->setObjectName(QStringLiteral("dangerButton"));
    auto *fileMkdirPath = new QLineEdit(fileTab);
    fileMkdirPath->setPlaceholderText(QStringLiteral("远端目录"));
    auto *fileMkdirBtn = new QPushButton(QStringLiteral("创建远端目录"), fileTab);
    auto *fileChmodPath = new QLineEdit(fileTab);
    fileChmodPath->setPlaceholderText(QStringLiteral("远端路径"));
    auto *fileChmodMode = new QLineEdit(fileTab);
    fileChmodMode->setPlaceholderText(QStringLiteral("例如 0755"));
    fileChmodMode->setMaximumWidth(110);
    auto *fileChmodBtn = new QPushButton(QStringLiteral("修改权限"), fileTab);
    fileGrid->addWidget(new QLabel(QStringLiteral("下载  远端"), fileTab), 0, 0);
    fileGrid->addWidget(fileFetchSrc, 0, 1, 1, 2);
    fileGrid->addWidget(new QLabel(QStringLiteral("本地"), fileTab), 0, 3);
    fileGrid->addWidget(fileFetchDest, 0, 4);
    fileGrid->addWidget(fileFetchBtn, 0, 5);
    fileGrid->addWidget(new QLabel(QStringLiteral("删除"), fileTab), 1, 0);
    fileGrid->addWidget(fileDeletePath, 1, 1, 1, 4);
    fileGrid->addWidget(fileDeleteBtn, 1, 5);
    fileGrid->addWidget(new QLabel(QStringLiteral("建目录"), fileTab), 2, 0);
    fileGrid->addWidget(fileMkdirPath, 2, 1, 1, 4);
    fileGrid->addWidget(fileMkdirBtn, 2, 5);
    fileGrid->addWidget(new QLabel(QStringLiteral("权限"), fileTab), 3, 0);
    fileGrid->addWidget(fileChmodPath, 3, 1, 1, 2);
    fileGrid->addWidget(new QLabel(QStringLiteral("模式"), fileTab), 3, 3);
    fileGrid->addWidget(fileChmodMode, 3, 4);
    fileGrid->addWidget(fileChmodBtn, 3, 5);
    fileGrid->setRowStretch(4, 1);
    connect(fileFetchBtn, &QPushButton::clicked, this, [this, fileFetchSrc, fileFetchDest, runAdHoc] {
        if (fileFetchSrc->text().trimmed().isEmpty() || fileFetchDest->text().trimmed().isEmpty()) {
            appendLog(QStringLiteral("\n请同时填写远端和本地路径。\n")); return;
        }
        runAdHoc(QStringLiteral("fetch"), QStringLiteral("src=%1 dest=%2 flat=no").arg(fileFetchSrc->text().trimmed(), fileFetchDest->text().trimmed()));
    });
    connect(fileDeleteBtn, &QPushButton::clicked, this, [this, fileDeletePath, runAdHoc] {
        const QString p = fileDeletePath->text().trimmed();
        if (p.isEmpty()) { appendLog(QStringLiteral("\n请先填写远端路径。\n")); return; }
        runAdHoc(QStringLiteral("file"), QStringLiteral("path=%1 state=absent").arg(p));
    });
    connect(fileMkdirBtn, &QPushButton::clicked, this, [this, fileMkdirPath, runAdHoc] {
        const QString p = fileMkdirPath->text().trimmed();
        if (p.isEmpty()) { appendLog(QStringLiteral("\n请先填写远端目录。\n")); return; }
        runAdHoc(QStringLiteral("file"), QStringLiteral("path=%1 state=directory").arg(p));
    });
    connect(fileChmodBtn, &QPushButton::clicked, this, [this, fileChmodPath, fileChmodMode, runAdHoc] {
        const QString p = fileChmodPath->text().trimmed();
        const QString m = fileChmodMode->text().trimmed();
        if (p.isEmpty() || m.isEmpty()) { appendLog(QStringLiteral("\n请同时填写路径和模式。\n")); return; }
        runAdHoc(QStringLiteral("file"), QStringLiteral("path=%1 mode=%2").arg(p, m));
    });
    commonSubTabs->addTab(fileTab, QStringLiteral("文件操作"));

    // ---------- 软件包 ----------
    auto *pkgTab = new QWidget(commonSubTabs);
    auto *pkgGrid = new QGridLayout(pkgTab);
    pkgGrid->setContentsMargins(16, 16, 16, 16);
    pkgGrid->setHorizontalSpacing(10);
    pkgGrid->setVerticalSpacing(10);
    pkgGrid->setColumnStretch(0, 0);
    for (int c = 1; c <= 4; ++c) pkgGrid->setColumnStretch(c, 1);
    auto *pkgNameEdit = new QLineEdit(pkgTab);
    pkgNameEdit->setPlaceholderText(QStringLiteral("软件包名，例如 vim、htop"));
    auto *pkgAptInstall = new QPushButton(QStringLiteral("apt 安装"), pkgTab);
    auto *pkgAptRemove = new QPushButton(QStringLiteral("apt 卸载"), pkgTab);
    auto *pkgYumInstall = new QPushButton(QStringLiteral("yum 安装"), pkgTab);
    auto *pkgYumRemove = new QPushButton(QStringLiteral("yum 卸载"), pkgTab);
    auto *pkgAutoInstall = new QPushButton(QStringLiteral("自动识别 (package) 安装"), pkgTab);
    pkgAutoInstall->setObjectName(QStringLiteral("primaryButton"));
    pkgGrid->addWidget(new QLabel(QStringLiteral("软件包"), pkgTab), 0, 0);
    pkgGrid->addWidget(pkgNameEdit, 0, 1, 1, 4);
    pkgGrid->addWidget(pkgAutoInstall, 1, 0, 1, 5);
    pkgGrid->addWidget(pkgAptInstall, 2, 0);
    pkgGrid->addWidget(pkgAptRemove, 2, 1);
    pkgGrid->addWidget(pkgYumInstall, 2, 2);
    pkgGrid->addWidget(pkgYumRemove, 2, 3);
    pkgGrid->setRowStretch(3, 1);
    auto runPkg = [this, pkgNameEdit, runAdHoc](const QString &module, const QString &state) {
        const QString name = pkgNameEdit->text().trimmed();
        if (name.isEmpty()) { appendLog(QStringLiteral("\n请先填写软件包名。\n")); return; }
        runAdHoc(module, QStringLiteral("name=%1 state=%2").arg(name, state));
    };
    connect(pkgAptInstall, &QPushButton::clicked, this, [runPkg] { runPkg(QStringLiteral("apt"), QStringLiteral("present")); });
    connect(pkgAptRemove, &QPushButton::clicked, this, [runPkg] { runPkg(QStringLiteral("apt"), QStringLiteral("absent")); });
    connect(pkgYumInstall, &QPushButton::clicked, this, [runPkg] { runPkg(QStringLiteral("yum"), QStringLiteral("present")); });
    connect(pkgYumRemove, &QPushButton::clicked, this, [runPkg] { runPkg(QStringLiteral("yum"), QStringLiteral("absent")); });
    connect(pkgAutoInstall, &QPushButton::clicked, this, [runPkg] { runPkg(QStringLiteral("package"), QStringLiteral("present")); });
    commonSubTabs->addTab(pkgTab, QStringLiteral("软件包"));

    tabs->addTab(commonTab, QStringLiteral("常用操作"));
    tabs->addTab(playbookTab, QStringLiteral("批量部署"));
    tabs->addTab(commandTab, QStringLiteral("批量命令"));
    tabs->addTab(copyTab, QStringLiteral("批量上传"));
    tabs->setCurrentIndex(0);

    auto *output = new QGroupBox(QStringLiteral("命令预览与日志"), central);
    auto *outputLayout = new QVBoxLayout(output);
    outputLayout->setContentsMargins(14, 18, 14, 14);
    outputLayout->setSpacing(10);
    previewEdit = new QPlainTextEdit(output);
    previewEdit->setReadOnly(true);
    previewEdit->setMinimumHeight(92);
    logEdit = new QPlainTextEdit(output);
    logEdit->setReadOnly(true);
    logEdit->setMinimumHeight(360);
    logEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    {
        QFont logFont = logEdit->font();
        logFont.setFamily(QStringLiteral("Monospace"));
        logFont.setStyleHint(QFont::TypeWriter);
        logEdit->setFont(logFont);
    }

    historyTable = new QTableWidget(0, 4, output);
    historyTable->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("命令"), QStringLiteral("退出码"), QStringLiteral("状态")});
    historyTable->verticalHeader()->setVisible(false);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    historyTable->horizontalHeader()->setStretchLastSection(false);
    historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    historyTable->setMinimumHeight(80);
    auto *historyContainer = new QWidget(output);
    auto *historyLayout = new QVBoxLayout(historyContainer);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->setSpacing(6);
    auto *historyButtons = new QHBoxLayout();
    historyButtons->setContentsMargins(0, 0, 0, 0);
    auto *historyCopyBtn = new QPushButton(QStringLiteral("复制选中命令"), historyContainer);
    auto *historyClearBtn = new QPushButton(QStringLiteral("清空历史"), historyContainer);
    historyButtons->addStretch(1);
    historyButtons->addWidget(historyCopyBtn);
    historyButtons->addWidget(historyClearBtn);
    historyLayout->addWidget(historyTable, 1);
    historyLayout->addLayout(historyButtons);
    connect(historyClearBtn, &QPushButton::clicked, this, [this] {
        historyTable->setRowCount(0);
//        currentHistoryRow = -1;
    });
    connect(historyCopyBtn, &QPushButton::clicked, this, [this] {
        const int row = historyTable->currentRow();
        if (row < 0) return;
        const auto *item = historyTable->item(row, 1);
        if (!item) return;
        QApplication::clipboard()->setText(item->text());
        appendLog(QStringLiteral("\n已复制命令到剪贴板。\n"));
    });
    connect(historyTable, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *item) {
        if (!item) return;
        const auto *cmdItem = historyTable->item(item->row(), 1);
        if (!cmdItem) return;
        QApplication::clipboard()->setText(cmdItem->text());
        appendLog(QStringLiteral("\n已复制命令到剪贴板。\n"));
    });

    auto *topTabs = new QTabWidget(output);
    topTabs->setDocumentMode(true);
    topTabs->addTab(previewEdit, QStringLiteral("命令预览"));
    topTabs->addTab(historyContainer, QStringLiteral("运行历史"));

    stopButton = new QPushButton(QStringLiteral("停止当前任务"), output);
    stopButton->setObjectName(QStringLiteral("dangerButton"));
    stopButton->setEnabled(false);
    auto *outputSplitter = new QSplitter(Qt::Vertical, output);
    outputSplitter->addWidget(topTabs);
    outputSplitter->addWidget(logEdit);
    outputSplitter->setStretchFactor(0, 0);
    outputSplitter->setStretchFactor(1, 1);
    outputSplitter->setSizes({120, 800});
    outputLayout->addWidget(outputSplitter, 1);
    outputLayout->addWidget(stopButton);

    root->addWidget(settings);
    root->addWidget(tabs);
    root->addWidget(output, 2);
    setCentralWidget(central);

    connect(workspaceButton, &QPushButton::clicked, this, [this] { const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择 Ansible 工作目录"), workspace()); if (!d.isEmpty()) { workspaceEdit->setText(cleanPath(d)); scanFiles(); } });
    connect(inventoryButton, &QPushButton::clicked, this, [this] { const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择 Inventory"), workspace()); if (!f.isEmpty()) inventoryCombo->setCurrentText(cleanPath(f)); });
    connect(playbookButton, &QPushButton::clicked, this, [this] { const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择 Playbook"), workspace(), QStringLiteral("YAML (*.yaml *.yml)")); if (!f.isEmpty()) playbookCombo->setCurrentText(cleanPath(f)); });
    connect(localButton, &QPushButton::clicked, this, [this] { QString p = QFileDialog::getExistingDirectory(this, QStringLiteral("选择目录"), workspace()); if (p.isEmpty()) p = QFileDialog::getOpenFileName(this, QStringLiteral("选择文件"), workspace()); if (!p.isEmpty()) localPathEdit->setText(cleanPath(p)); });
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::scanFiles);
    connect(pingButton, &QPushButton::clicked, this, [this] { QStringList a{QStringLiteral("-i"), inventory(), group(), QStringLiteral("-m"), QStringLiteral("ping")}; a += commonArgs(); runInExternalTerminal(QStringLiteral("ansible"), a, workspace()); });
    connect(runPlaybookButton, &QPushButton::clicked, this, [this] { const QString pb = playbookCombo->currentText(); QStringList a{QStringLiteral("-i"), inventory(), pb}; a += commonArgs(); runInExternalTerminal(QStringLiteral("ansible-playbook"), a, QFileInfo(pb).absolutePath()); });
    connect(runCommandButton, &QPushButton::clicked, this, [this] { QStringList a{QStringLiteral("-i"), inventory(), group(), QStringLiteral("-m"), moduleCombo->currentText(), QStringLiteral("-a"), commandEdit->toPlainText().trimmed()}; a += commonArgs(); runInExternalTerminal(QStringLiteral("ansible"), a, workspace()); });
    connect(runCopyButton, &QPushButton::clicked, this, [this] { QStringList a{QStringLiteral("-i"), inventory(), group(), QStringLiteral("-m"), QStringLiteral("copy"), QStringLiteral("-a"), QStringLiteral("src=%1 dest=%2").arg(localPathEdit->text(), remotePathEdit->text())}; a += commonArgs(); runInExternalTerminal(QStringLiteral("ansible"), a, workspace()); });
    connect(stopButton, &QPushButton::clicked, this, [this] {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
        if (activeRuns.isEmpty()) {
            appendLog(QStringLiteral("\n当前没有正在运行的外部任务。\n"));
            stopButton->setEnabled(false);
            return;
        }
        appendLog(QStringLiteral("\n正在停止 %1 个外部终端任务...\n").arg(activeRuns.size()));
        for (const ExternalRun &run : activeRuns) {
            QFile pidFile(run.pidPath);
            if (!pidFile.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString pidText = QString::fromLocal8Bit(pidFile.readAll()).trimmed();
            pidFile.close();
            bool ok = false;
            const qint64 pid = pidText.toLongLong(&ok);
            if (!ok || pid <= 0) continue;
            // 杀整个进程组，包括 ansible 子进程
            QProcess::execute(QStringLiteral("bash"),
                              {QStringLiteral("-c"),
                               QStringLiteral("kill -TERM -%1 2>/dev/null; sleep 1; kill -KILL -%1 2>/dev/null; "
                                              "kill -TERM %1 2>/dev/null; kill -KILL %1 2>/dev/null; true")
                                   .arg(pid)});
            appendLog(QStringLiteral("已向 PID %1 发送终止信号。\n").arg(pid));
        }
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this] { appendLog(QString::fromLocal8Bit(process->readAllStandardOutput())); });
    connect(process, &QProcess::readyReadStandardError, this, [this] { appendLog(QString::fromLocal8Bit(process->readAllStandardError())); });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int code) { appendLog(QStringLiteral("\n任务结束，退出码：%1\n").arg(code)); stopButton->setEnabled(false); });
    for (auto *line : {workspaceEdit, limitEdit, extraArgsEdit, localPathEdit, remotePathEdit}) connect(line, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(inventoryCombo, &QComboBox::currentTextChanged, this, &MainWindow::updatePreview);
    connect(groupCombo, &QComboBox::currentTextChanged, this, &MainWindow::updatePreview);
    connect(playbookCombo, &QComboBox::currentTextChanged, this, &MainWindow::updatePreview);
    connect(moduleCombo, &QComboBox::currentTextChanged, this, &MainWindow::updatePreview);
    connect(commandEdit, &QPlainTextEdit::textChanged, this, &MainWindow::updatePreview);
}

void MainWindow::scanFiles()
{
    inventoryCombo->clear();
    playbookCombo->clear();

    const QString systemInventory = QStringLiteral("/etc/ansible/hosts");
    if (QFileInfo::exists(systemInventory)) {
        inventoryCombo->addItem(systemInventory);
    }

    const QString root = workspace();
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = cleanPath(it.next());
        const QFileInfo info(file);
        const QString name = info.fileName().toLower();

        if (name.endsWith(QStringLiteral(".yaml")) || name.endsWith(QStringLiteral(".yml"))) {
            playbookCombo->addItem(file);
        }

        if (name == QStringLiteral("hosts") || name == QStringLiteral("my_hosts") || name.endsWith(QStringLiteral(".ini"))) {
            if (inventoryCombo->findText(file) < 0) {
                inventoryCombo->addItem(file);
            }
        }
    }

    const int systemInventoryIndex = inventoryCombo->findText(systemInventory);
    if (systemInventoryIndex >= 0) {
        inventoryCombo->setCurrentIndex(systemInventoryIndex);
    } else {
        const QString defaultInventory = cleanPath(QDir(root).absoluteFilePath(QStringLiteral("hosts")));
        if (QFileInfo::exists(defaultInventory)) {
            const int index = inventoryCombo->findText(defaultInventory);
            if (index >= 0) {
                inventoryCombo->setCurrentIndex(index);
            } else {
                inventoryCombo->insertItem(0, defaultInventory);
                inventoryCombo->setCurrentIndex(0);
            }
        }
    }

    updatePreview();
}

void MainWindow::runProcess(const QString &program, const QStringList &arguments, const QString &workingDirectory)
{
    if (process->state() != QProcess::NotRunning) {
        appendLog(QStringLiteral("已有任务正在运行，请先停止或等待完成。\n"));
        return;
    }

    if (!ensureSharedMemoryAvailable()) {
        return;
    }

    QStringList realArguments = arguments;
    if (!realArguments.contains(QStringLiteral("--forks")) && !realArguments.contains(QStringLiteral("-f"))) {
        realArguments << QStringLiteral("--forks") << QStringLiteral("1");
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString ansibleTemp = QDir(tempRoot).absoluteFilePath(QStringLiteral("ansible-pilot"));
    QDir().mkpath(ansibleTemp);
    env.insert(QStringLiteral("TMPDIR"), tempRoot);
    env.insert(QStringLiteral("ANSIBLE_LOCAL_TEMP"), ansibleTemp);
    env.insert(QStringLiteral("ANSIBLE_REMOTE_TEMP"), QStringLiteral("~/.ansible/tmp"));

    appendLog(QStringLiteral("\n[%1] 执行：%2 %3\n")
                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                       program,
                       realArguments.join(QLatin1Char(' '))));
    appendLog(QStringLiteral("工作目录：%1\n").arg(workingDirectory));
    appendLog(QStringLiteral("PATH：%1\n").arg(env.value(QStringLiteral("PATH"))));
    appendLog(QStringLiteral("ANSIBLE_LOCAL_TEMP：%1\n").arg(env.value(QStringLiteral("ANSIBLE_LOCAL_TEMP"))));

    process->setProcessEnvironment(env);
    process->setWorkingDirectory(workingDirectory);
    stopButton->setEnabled(true);
    process->start(program, realArguments);
}

void MainWindow::runInExternalTerminal(const QString &program, const QStringList &arguments, const QString &workingDirectory)
{
    QStringList realArguments = arguments;
    if (!realArguments.contains(QStringLiteral("--forks")) && !realArguments.contains(QStringLiteral("-f"))) {
        realArguments << QStringLiteral("--forks") << QStringLiteral("1");
    }

    QString command = shellQuote(program);
    for (const QString &argument : realArguments) {
        command += QLatin1Char(' ');
        command += shellQuote(argument);
    }

    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString ansibleTemp = QDir(tempRoot).absoluteFilePath(QStringLiteral("ansible-pilot"));
    QDir().mkpath(ansibleTemp);

    // 每次执行使用独立的脚本/日志/状态/PID 文件名，避免并发任务互相覆盖。
    const QString runId = QString::number(QDateTime::currentMSecsSinceEpoch())
                          + QStringLiteral("-") + QString::number(QCoreApplication::applicationPid())
                          + QStringLiteral("-") + QString::number(activeRuns.size());
    const QString scriptPath = QDir(tempRoot).absoluteFilePath(QStringLiteral("ansible-pilot-run-%1.sh").arg(runId));
    const QString logPath = QDir(tempRoot).absoluteFilePath(QStringLiteral("ansible-pilot-output-%1.log").arg(runId));
    const QString statusPath = QDir(tempRoot).absoluteFilePath(QStringLiteral("ansible-pilot-status-%1.txt").arg(runId));
    const QString pidPath = QDir(tempRoot).absoluteFilePath(QStringLiteral("ansible-pilot-pid-%1.txt").arg(runId));

    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        appendLog(QStringLiteral("无法创建终端执行脚本：%1\n").arg(scriptPath));
        return;
    }

    const QString scriptContent = QStringLiteral(
        "#!/bin/bash\n"
        "set -o pipefail\n"
        "LOG_FILE=%5\n"
        "STATUS_FILE=%6\n"
        "PID_FILE=%7\n"
        ": > \"${LOG_FILE}\"\n"
        "rm -f \"${STATUS_FILE}\"\n"
        "echo $$ > \"${PID_FILE}\"\n"
        "exec > >(tee -a \"${LOG_FILE}\") 2>&1\n"
        "cd %1 || exit 1\n"
        "export TMPDIR=%2\n"
        "export ANSIBLE_LOCAL_TEMP=%3\n"
        "export ANSIBLE_REMOTE_TEMP='~/.ansible/tmp'\n"
        "echo '========== AnsiblePilot 终端执行 =========='\n"
        "echo \"工作目录：$(pwd)\"\n"
        "echo \"当前用户：$(id -un)\"\n"
        "echo \"PID：$$\"\n"
        "echo '---------- Ansible 命令 ----------'\n"
        "echo %4\n"
        "echo\n"
        "%4\n"
        "STATUS=$?\n"
        "echo\n"
        "echo \"任务结束，退出码：${STATUS}\"\n"
        "echo ${STATUS} > \"${STATUS_FILE}\"\n"
        "echo '按回车关闭终端...'\n"
        "read _ || true\n"
        "exit ${STATUS}\n")
        .arg(shellQuote(workingDirectory),
             shellQuote(tempRoot),
             shellQuote(ansibleTemp),
             command,
             shellQuote(logPath),
             shellQuote(statusPath),
             shellQuote(pidPath));

    script.write(scriptContent.toLocal8Bit());
    script.close();
    QFile::setPermissions(scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                      QFile::ReadGroup | QFile::ExeGroup |
                                      QFile::ReadOther | QFile::ExeOther);

    appendLog(QStringLiteral("\n[%1] 在外部终端执行：%2\n")
                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                       command));
    appendLog(QStringLiteral("终端脚本：%1\n").arg(scriptPath));
    appendLog(QStringLiteral("同步日志：%1\n").arg(logPath));

    ExternalRun run;
    run.logPath = logPath;
    run.statusPath = statusPath;
    run.pidPath = pidPath;
    run.logOffset = 0;
    run.historyRow = recordHistoryStart(command);

    const QList<QPair<QString, QStringList>> terminals{
        {QStringLiteral("x-terminal-emulator"), {QStringLiteral("-e"), QStringLiteral("bash"), scriptPath}},
        {QStringLiteral("mate-terminal"), {QStringLiteral("--"), QStringLiteral("bash"), scriptPath}},
        {QStringLiteral("gnome-terminal"), {QStringLiteral("--"), QStringLiteral("bash"), scriptPath}},
        {QStringLiteral("konsole"), {QStringLiteral("-e"), QStringLiteral("bash"), scriptPath}},
        {QStringLiteral("xfce4-terminal"), {QStringLiteral("--command"), QStringLiteral("bash ") + shellQuote(scriptPath)}},
        {QStringLiteral("xterm"), {QStringLiteral("-e"), QStringLiteral("bash"), scriptPath}}
    };

    bool launched = false;
    for (const auto &terminal : terminals) {
        if (QProcess::startDetached(terminal.first, terminal.second)) {
            appendLog(QStringLiteral("已打开外部终端：%1\n").arg(terminal.first));
            launched = true;
            break;
        }
    }

    if (!launched) {
        appendLog(QStringLiteral(
            "无法自动打开外部终端。请手动在普通终端执行：\n"
            "bash %1\n").arg(shellQuote(scriptPath)));
    }

    activeRuns.append(run);
    stopButton->setEnabled(true);
    if (!externalLogTimer->isActive()) {
        externalLogTimer->start();
    }
}

bool MainWindow::ensureSharedMemoryAvailable()
{
    const QString shmPath = QStringLiteral("/dev/shm");
    const QFileInfo shmInfo(shmPath);

    if (!shmInfo.exists() || !shmInfo.isDir()) {
        appendLog(QStringLiteral(
            "\n错误：当前系统不存在 /dev/shm，Ansible 无法创建 multiprocessing 所需的共享内存对象。\n"
            "请在终端检查：ls -ld /dev/shm\n"));
        return false;
    }

    const QString testPath = QDir(shmPath).absoluteFilePath(QStringLiteral("ansible_pilot_shm_test_%1").arg(QDateTime::currentMSecsSinceEpoch()));
    QFile testFile(testPath);
    if (!testFile.open(QIODevice::WriteOnly)) {
        appendLog(QStringLiteral(
            "\n错误：当前程序无法写入 /dev/shm，这会导致 Ansible 报错：\n"
            "ERROR! Unable to use multiprocessing, this is normally caused by lack of access to /dev/shm: [Errno 13] Permission denied\n\n"
            "请在启动本程序的同一用户环境下检查：\n"
            "ls -ld /dev/shm\n"
            "df -h /dev/shm\n\n"
            "正常权限通常应该类似：drwxrwxrwt，也就是 1777。\n"
            "可联系管理员或在终端执行：sudo chmod 1777 /dev/shm\n"));
        return false;
    }

    testFile.write("ok");
    testFile.close();
    QFile::remove(testPath);

    QProcess pythonCheck;
    pythonCheck.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    pythonCheck.start(QStringLiteral("python3"), {
        QStringLiteral("-c"),
        QStringLiteral("import multiprocessing as mp; q=mp.Queue(); q.put('ok'); assert q.get() == 'ok'")
    });

    if (!pythonCheck.waitForFinished(5000) || pythonCheck.exitCode() != 0) {
        const QString error = QString::fromLocal8Bit(pythonCheck.readAllStandardError()).trimmed();
        appendLog(QStringLiteral(
            "\n错误：/dev/shm 文件权限看起来正常，但 Python multiprocessing 自检失败。\n"
            "这和 Ansible 报错属于同一类问题，通常是当前运行环境限制了 POSIX semaphore 或共享内存对象。\n\n"
            "请在同一个终端里执行下面命令确认：\n"
            "python3 -c \"import multiprocessing as mp; q=mp.Queue(); q.put('ok'); print(q.get())\"\n\n"
            "Python 错误信息：%1\n\n"
            "如果终端成功但 GUI 失败，请从终端启动本程序；如果终端也失败，需要检查系统 /dev/shm 挂载、安全策略或容器/沙箱限制。\n")
                      .arg(error.isEmpty() ? QStringLiteral("无详细错误输出") : error));
        return false;
    }

    return true;
}

void MainWindow::pollExternalTerminalOutput()
{
    for (int i = 0; i < activeRuns.size(); ) {
        ExternalRun &run = activeRuns[i];

        QFile logFile(run.logPath);
        if (logFile.open(QIODevice::ReadOnly)) {
            if (run.logOffset > logFile.size()) {
                run.logOffset = 0;
            }
            if (logFile.seek(run.logOffset)) {
                const QByteArray data = logFile.readAll();
                run.logOffset = logFile.pos();
                if (!data.isEmpty()) {
                    appendLog(QString::fromLocal8Bit(data));
                }
            }
        }

        if (QFileInfo::exists(run.statusPath)) {
            QFile statusFile(run.statusPath);
            QString status;
            if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                status = QString::fromLocal8Bit(statusFile.readAll()).trimmed();
            }
            appendLog(QStringLiteral("\n外部终端任务结束，退出码：%1\n").arg(status.isEmpty() ? QStringLiteral("未知") : status));
            const QString stateText = (status == QStringLiteral("0")) ? QStringLiteral("成功")
                                      : status.isEmpty() ? QStringLiteral("未知")
                                      : QStringLiteral("失败");
            recordHistoryFinish(run.historyRow, stateText, status);
            QFile::remove(run.pidPath);
            activeRuns.removeAt(i);
            continue;
        }
        ++i;
    }

    if (activeRuns.isEmpty()) {
        externalLogTimer->stop();
        stopButton->setEnabled(false);
    }
}

int MainWindow::recordHistoryStart(const QString &command)
{
    if (!historyTable) return -1;
    const int row = historyTable->rowCount();
    historyTable->insertRow(row);
    auto *timeItem = new QTableWidgetItem(QDateTime::currentDateTime().toString(QStringLiteral("MM-dd HH:mm:ss")));
    auto *cmdItem = new QTableWidgetItem(command);
    cmdItem->setToolTip(command);
    auto *codeItem = new QTableWidgetItem(QString());
    auto *stateItem = new QTableWidgetItem(QStringLiteral("运行中"));
    stateItem->setForeground(QBrush(QColor("#1d4ed8")));
    historyTable->setItem(row, 0, timeItem);
    historyTable->setItem(row, 1, cmdItem);
    historyTable->setItem(row, 2, codeItem);
    historyTable->setItem(row, 3, stateItem);
    historyTable->scrollToBottom();
    historyTable->selectRow(row);
    return row;
}

void MainWindow::recordHistoryFinish(int row, const QString &status, const QString &exitCode)
{
    if (!historyTable || row < 0 || row >= historyTable->rowCount()) return;
    if (auto *codeItem = historyTable->item(row, 2)) {
        codeItem->setText(exitCode);
    }
    if (auto *stateItem = historyTable->item(row, 3)) {
        stateItem->setText(status);
        if (status == QStringLiteral("成功")) {
            stateItem->setForeground(QBrush(QColor("#15803d")));
        } else if (status == QStringLiteral("失败")) {
            stateItem->setForeground(QBrush(QColor("#b91c1c")));
        } else {
            stateItem->setForeground(QBrush(QColor("#a16207")));
        }
    }
}

void MainWindow::appendLog(const QString &message)
{
    QScrollBar *bar = logEdit->verticalScrollBar();
    const bool atBottom = !bar || bar->value() >= bar->maximum() - 4;
    logEdit->moveCursor(QTextCursor::End);
    logEdit->insertPlainText(message);
    if (atBottom) {
        logEdit->moveCursor(QTextCursor::End);
        logEdit->ensureCursorVisible();
        if (bar) bar->setValue(bar->maximum());
    }
}

void MainWindow::updatePreview()
{
    QStringList lines;
    lines << QStringLiteral("Playbook: %1 -i %2 %3 %4")
                 .arg(QStringLiteral("ansible-playbook"),
                      inventory(),
                      playbookCombo ? playbookCombo->currentText() : QString(),
                      commonArgs().join(QLatin1Char(' ')));
    lines << QStringLiteral("命令: %1 -i %2 %3 -m %4 -a \"%5\" %6")
                 .arg(QStringLiteral("ansible"),
                      inventory(),
                      group(),
                      moduleCombo ? moduleCombo->currentText() : QStringLiteral("shell"),
                      commandEdit ? commandEdit->toPlainText().trimmed() : QString(),
                      commonArgs().join(QLatin1Char(' ')));
    lines << QStringLiteral("上传: %1 -i %2 %3 -m copy -a \"src=%4 dest=%5\" %6")
                 .arg(QStringLiteral("ansible"),
                      inventory(),
                      group(),
                      localPathEdit ? localPathEdit->text() : QString(),
                      remotePathEdit ? remotePathEdit->text() : QString(),
                      commonArgs().join(QLatin1Char(' ')));

    previewEdit->setPlainText(lines.join(QLatin1Char('\n')));
}

QString MainWindow::workspace() const
{
    return cleanPath(workspaceEdit ? workspaceEdit->text().trimmed() : QDir::currentPath());
}

QString MainWindow::inventory() const
{
    return inventoryCombo ? inventoryCombo->currentText().trimmed() : QString();
}

QString MainWindow::group() const
{
    const QString value = groupCombo ? groupCombo->currentText().trimmed() : QString();
    return value.isEmpty() ? QStringLiteral("all") : value;
}

QString MainWindow::shellQuote(const QString &text) const
{
    QString escaped = text;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\"'\"'"));
    return QStringLiteral("'%1'").arg(escaped);
}

QStringList MainWindow::commonArgs() const
{
    QStringList args;

    if (limitEdit && !limitEdit->text().trimmed().isEmpty()) {
        args << QStringLiteral("--limit") << limitEdit->text().trimmed();
    }

    if (extraArgsEdit && !extraArgsEdit->text().trimmed().isEmpty()) {
        args += splitArgs(extraArgsEdit->text().trimmed());
    }

    return args;
}

QStringList MainWindow::splitArgs(const QString &text) const
{
    QStringList result;
    QString current;
    bool inQuotes = false;

    for (const QChar ch : text) {
        if (ch == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            continue;
        }

        if (ch.isSpace() && !inQuotes) {
            if (!current.isEmpty()) {
                result << current;
                current.clear();
            }
            continue;
        }

        current.append(ch);
    }

    if (!current.isEmpty()) {
        result << current;
    }

    return result;
}
