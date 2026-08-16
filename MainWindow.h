#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QStringList>
#include <QDateTime>

namespace Ui {
class MainWindow;
}

class QTimer;

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

private:
    void setupUiStyles();
    void scanFiles();
    void updateInventoryGroups();
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
