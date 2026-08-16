#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QStringList>
#include <QDateTime>
#include <QList>

namespace Ui {
class MainWindow;
}

class QTimer;

struct PairInfo {
    int pairId = 0;
    QString serviceName;
    QString softwareName;
    QString masterIp;
    QString slaveIp;
};

struct ProcessPreset {
    QString name;
    QString keyword;
    QString logPath;
    int pairId = 0;
    QString desc;
    QString pidQuery;
};

struct CommandPreset {
    QString category;
    QString name;
    QString script;
    QString target;
    QString module = QStringLiteral("shell");
    QString desc;
    int pairId = 0;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onMainTabChanged(int index);
    void onOpsSubTabChanged(int index);
    void onInventorySelected(int index);
    void onSearchLog(const QString &keyword);
    void clearLogs();
    void exportLogs();
    void copyLogs();

    void onPresetProcChanged(int index);
    void applyPresetProc();
    void onPresetCmdChanged(int index);
    void applyPresetCmd();

    void loadInventoryPreview();
    void saveInventoryPreview();
    void copyInventoryPreview();
    void openInventoryPreviewTab();

    void loadPlaybookPreview();
    void savePlaybookPreview();
    void copyPlaybookPreview();
    void openPlaybookPreviewTab();

private:
    void setupUiStyles();
    void scanFiles();
    void updateInventoryGroups();
    void loadClusterConfig();
    void runProcess(const QString &program, const QStringList &arguments, const QString &workingDirectory);
    void runInExternalTerminal(const QString &program, const QStringList &arguments, const QString &workingDirectory);
    void appendLog(const QString &message);
    void updatePreview();
    void pollExternalTerminalOutput();
    int recordHistoryStart(const QString &command);
    void recordHistoryFinish(int row, const QString &status, const QString &exitCode, qint64 durationMs = 0);
    void cleanupOldTempFiles();
    
    QString ansiToHtml(const QString &text) const;
    QString shellQuote(const QString &text) const;
    QString workspace() const;
    QString inventory() const;
    QString group() const;
    QString limit() const;
    QString extraArgs() const;
    QStringList commonArgs() const;
    QStringList ansibleArgs(const QString &module = QString(), const QString &moduleArgs = QString()) const;
    QStringList playbookArgs(const QString &playbook) const;
    QStringList splitArgs(const QString &text) const;

    Ui::MainWindow *ui;

    QProcess *process = nullptr;
    QTimer *externalLogTimer = nullptr;

    QList<PairInfo> pairInfos;
    QList<ProcessPreset> processPresets;
    QList<CommandPreset> commandPresets;

    struct ExternalRun {
        QString logPath;
        QString statusPath;
        QString pidPath;
        qint64 logOffset = 0;
        int historyRow = -1;
        QDateTime startTime;
    };
    QList<ExternalRun> activeRuns;
};
