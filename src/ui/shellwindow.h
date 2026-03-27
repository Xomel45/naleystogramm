#pragma once
#include <QDialog>
#include <QString>

class QPlainTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;

// ── ShellWindow ───────────────────────────────────────────────────────────────
// Окно инициатора удалённой шелл-сессии.
// Отображает вывод удалённого шелла (stdout/stderr) и позволяет вводить команды.
// Ввод проверяется RemoteShellManager::sendInput() перед отправкой.

class ShellWindow : public QDialog {
    Q_OBJECT
public:
    explicit ShellWindow(const QString& sessionId,
                         const QString& peerName,
                         QWidget* parent = nullptr);

    [[nodiscard]] QString sessionId() const { return m_sessionId; }

    // Добавить данные stdout/stderr в терминальный дисплей
    void appendOutput(const QByteArray& data);

    // Показать сообщение о завершении сессии и заблокировать ввод
    void showSessionEnded(const QString& reason);

signals:
    // Пользователь отправил команду → RemoteShellManager::sendInput()
    void inputSubmitted(QString sessionId, QByteArray data);

    // Пользователь завершил сессию → RemoteShellManager::killSession()
    void terminateRequested(QString sessionId);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onInputEnter();

private:
    void setupUi(const QString& peerName);

    QString          m_sessionId;
    QPlainTextEdit*  m_output        {nullptr};
    QLineEdit*       m_input         {nullptr};
    QPushButton*     m_sendBtn       {nullptr};
    QPushButton*     m_terminateBtn  {nullptr};
    QLabel*          m_statusLbl     {nullptr};
    bool             m_ended         {false};
};
