#pragma once

#include <QList>
#include <QMainWindow>
#include <QProcess>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QTableWidget;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildUi();
    void scanFiles();
    void runProcess(const QString &program, const QStringList &arguments, const QString &workingDirectory);
    void runInExternalTerminal(const QString &program, const QStringList &arguments, const QString &workingDirectory);
    void appendLog(const QString &message);
    void updatePreview();
    void pollExternalTerminalOutput();
    int recordHistoryStart(const QString &command);
    void recordHistoryFinish(int row, const QString &status, const QString &exitCode);
    bool ensureSharedMemoryAvailable();
    QString shellQuote(const QString &text) const;
    QString workspace() const;
    QString inventory() const;
    QString group() const;
    QStringList commonArgs() const;
    QStringList splitArgs(const QString &text) const;

    QLineEdit *workspaceEdit = nullptr;
    QComboBox *inventoryCombo = nullptr;
    QComboBox *groupCombo = nullptr;
    QComboBox *playbookCombo = nullptr;
    QLineEdit *limitEdit = nullptr;
    QLineEdit *extraArgsEdit = nullptr;
    QComboBox *moduleCombo = nullptr;
    QPlainTextEdit *commandEdit = nullptr;
    QLineEdit *localPathEdit = nullptr;
    QLineEdit *remotePathEdit = nullptr;
    QPlainTextEdit *previewEdit = nullptr;
    QPlainTextEdit *logEdit = nullptr;
    QPushButton *stopButton = nullptr;
    QProcess *process = nullptr;
    QTimer *externalLogTimer = nullptr;
    struct ExternalRun {
        QString logPath;
        QString statusPath;
        QString pidPath;
        qint64 logOffset = 0;
        int historyRow = -1;
    };
    QList<ExternalRun> activeRuns;
    QTableWidget *historyTable = nullptr;
};
