#pragma once
#include <QDialog>
#include <QString>

class QPlainTextEdit;
class QPushButton;
class QLabel;

// ── ShellMonitor ──────────────────────────────────────────────────────────────
// Окно мониторинга шелл-сессии на стороне ПОЛУЧАТЕЛЯ.
// Только просмотр: отображает команды инициатора и вывод шелла.
// Содержит большую красную кнопку принудительного завершения.

class ShellMonitor : public QDialog {
    Q_OBJECT
public:
    explicit ShellMonitor(const QString& sessionId,
                          const QString& peerName,
                          QWidget* parent = nullptr);

    [[nodiscard]] QString sessionId() const { return m_sessionId; }

    // Добавить вывод stdout/stderr от процесса
    void appendData(const QByteArray& data);

    // Отобразить команду инициатора (с пометкой "> ")
    void appendInput(const QByteArray& cmd);

    // Показать завершение сессии и заблокировать кнопку
    void showSessionEnded(const QString& reason);

signals:
    // Получатель нажал "Завершить" → RemoteShellManager::killSession()
    void terminateRequested(QString sessionId);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi(const QString& peerName);

    QString          m_sessionId;
    QPlainTextEdit*  m_output       {nullptr};
    QPushButton*     m_terminateBtn {nullptr};
    QLabel*          m_statusLbl    {nullptr};
    bool             m_ended        {false};
};
