#include "MainWindow.h"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
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
    tabs->setMinimumHeight(210);
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

    tabs->addTab(playbookTab, QStringLiteral("批量部署"));
    tabs->addTab(commandTab, QStringLiteral("批量命令"));
    tabs->addTab(copyTab, QStringLiteral("批量上传"));

    auto *output = new QGroupBox(QStringLiteral("命令预览与日志"), central);
    auto *outputLayout = new QVBoxLayout(output);
    outputLayout->setContentsMargins(14, 18, 14, 14);
    outputLayout->setSpacing(10);
    previewEdit = new QPlainTextEdit(output);
    previewEdit->setReadOnly(true);
    previewEdit->setMinimumHeight(92);
    previewEdit->setMaximumHeight(120);
    logEdit = new QPlainTextEdit(output);
    logEdit->setReadOnly(true);
    logEdit->setMinimumHeight(240);
    logEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    stopButton = new QPushButton(QStringLiteral("停止当前任务"), output);
    stopButton->setObjectName(QStringLiteral("dangerButton"));
    stopButton->setEnabled(false);
    auto *outputSplitter = new QSplitter(Qt::Vertical, output);
    outputSplitter->addWidget(previewEdit);
    outputSplitter->addWidget(logEdit);
    outputSplitter->setStretchFactor(0, 1);
    outputSplitter->setStretchFactor(1, 5);
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
    connect(stopButton, &QPushButton::clicked, this, [this] { process->kill(); });
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
    if (externalLogTimer->isActive()) {
        externalLogTimer->stop();
    }

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
    const QString scriptPath = QDir(tempRoot).absoluteFilePath(QStringLiteral("ansible-pilot-run.sh"));
    const QString ansibleTemp = QDir(tempRoot).absoluteFilePath(QStringLiteral("ansible-pilot"));
    externalLogPath = QDir(tempRoot).absoluteFilePath(QStringLiteral("ansible-pilot-output.log"));
    externalStatusPath = QDir(tempRoot).absoluteFilePath(QStringLiteral("ansible-pilot-status.txt"));
    externalLogOffset = 0;
    QDir().mkpath(ansibleTemp);
    QFile::remove(externalLogPath);
    QFile::remove(externalStatusPath);

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
        ": > \"${LOG_FILE}\"\n"
        "rm -f \"${STATUS_FILE}\"\n"
        "exec > >(tee -a \"${LOG_FILE}\") 2>&1\n"
        "cd %1 || exit 1\n"
        "export TMPDIR=%2\n"
        "export ANSIBLE_LOCAL_TEMP=%3\n"
        "export ANSIBLE_REMOTE_TEMP='~/.ansible/tmp'\n"
        "echo '========== AnsiblePilot 终端执行 =========='\n"
        "echo \"工作目录：$(pwd)\"\n"
        "echo \"当前用户：$(id)\"\n"
        "echo \"PATH：$PATH\"\n"
        "echo \"TMPDIR：$TMPDIR\"\n"
        "echo \"ANSIBLE_LOCAL_TEMP：$ANSIBLE_LOCAL_TEMP\"\n"
        "echo '---------- /dev/shm ----------'\n"
        "ls -ld /dev/shm\n"
        "echo '---------- Python multiprocessing ----------'\n"
        "python3 -c \"import multiprocessing as mp; q=mp.Queue(); q.put('ok'); print(q.get())\"\n"
        "PY_STATUS=$?\n"
        "if [ ${PY_STATUS} -ne 0 ]; then\n"
        "    echo 'Python multiprocessing 自检失败，停止执行 Ansible。'\n"
        "    echo '按回车关闭终端...'\n"
        "    echo ${PY_STATUS} > \"${STATUS_FILE}\"\n"
        "    read\n"
        "    exit ${PY_STATUS}\n"
        "fi\n"
        "echo '---------- Ansible 命令 ----------'\n"
        "echo %4\n"
        "%4\n"
        "STATUS=$?\n"
        "echo\n"
        "echo \"任务结束，退出码：${STATUS}\"\n"
        "echo '按回车关闭终端...'\n"
        "echo ${STATUS} > \"${STATUS_FILE}\"\n"
        "read\n"
        "exit ${STATUS}\n")
        .arg(shellQuote(workingDirectory),
             shellQuote(tempRoot),
             shellQuote(ansibleTemp),
             command,
             shellQuote(externalLogPath),
             shellQuote(externalStatusPath));

    script.write(scriptContent.toLocal8Bit());
    script.close();
    QFile::setPermissions(scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                      QFile::ReadGroup | QFile::ExeGroup |
                                      QFile::ReadOther | QFile::ExeOther);

    appendLog(QStringLiteral("\n[%1] 在外部终端执行：%2\n")
                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                       command));
    appendLog(QStringLiteral("终端脚本：%1\n").arg(scriptPath));
    appendLog(QStringLiteral("同步日志：%1\n").arg(externalLogPath));
    appendLog(QStringLiteral("状态文件：%1\n").arg(externalStatusPath));

    const QList<QPair<QString, QStringList>> terminals{
        {QStringLiteral("x-terminal-emulator"), {QStringLiteral("-e"), QStringLiteral("bash"), scriptPath}},
        {QStringLiteral("mate-terminal"), {QStringLiteral("--"), QStringLiteral("bash"), scriptPath}},
        {QStringLiteral("gnome-terminal"), {QStringLiteral("--"), QStringLiteral("bash"), scriptPath}},
        {QStringLiteral("konsole"), {QStringLiteral("-e"), QStringLiteral("bash"), scriptPath}},
        {QStringLiteral("xfce4-terminal"), {QStringLiteral("--command"), QStringLiteral("bash ") + shellQuote(scriptPath)}},
        {QStringLiteral("xterm"), {QStringLiteral("-e"), QStringLiteral("bash"), scriptPath}}
    };

    for (const auto &terminal : terminals) {
        if (QProcess::startDetached(terminal.first, terminal.second)) {
            appendLog(QStringLiteral("已打开外部终端：%1\n").arg(terminal.first));
            externalLogTimer->start();
            return;
        }
    }

    appendLog(QStringLiteral(
        "无法自动打开外部终端。请手动在普通终端执行：\n"
        "bash %1\n").arg(shellQuote(scriptPath)));
    externalLogTimer->start();
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
    if (!externalLogPath.isEmpty()) {
        QFile logFile(externalLogPath);
        if (logFile.open(QIODevice::ReadOnly)) {
            if (externalLogOffset > logFile.size()) {
                externalLogOffset = 0;
            }

            if (logFile.seek(externalLogOffset)) {
                const QByteArray data = logFile.readAll();
                externalLogOffset = logFile.pos();
                if (!data.isEmpty()) {
                    appendLog(QString::fromLocal8Bit(data));
                }
            }
        }
    }

    if (!externalStatusPath.isEmpty() && QFileInfo::exists(externalStatusPath)) {
        QFile statusFile(externalStatusPath);
        if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString status = QString::fromLocal8Bit(statusFile.readAll()).trimmed();
            externalLogTimer->stop();
            appendLog(QStringLiteral("\n外部终端任务结束，退出码：%1\n").arg(status.isEmpty() ? QStringLiteral("未知") : status));
            externalStatusPath.clear();
        }
    }
}

void MainWindow::appendLog(const QString &message)
{
    logEdit->moveCursor(QTextCursor::End);
    logEdit->insertPlainText(message);
    logEdit->moveCursor(QTextCursor::End);
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
