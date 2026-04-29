#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;

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
};
