#pragma once
#include <QDialog>

class QProgressBar;
class QLabel;

// ── SplashScreen ───────────────────────────────────────────────────────────────
// Экран загрузки приложения. Показывается до создания главного окна.
// Обновляется через updateStatus(progress, status) на каждом шаге инициализации.
// Закрывается явно: вызовом hide() из main() после show() главного окна.

class SplashScreen : public QDialog {
    Q_OBJECT
public:
    explicit SplashScreen(QWidget* parent = nullptr);

    // Обновить прогресс (0–100) и строку статуса.
    // Также обновляет забавную фразу каждые 25 единиц прогресса.
    void updateStatus(int progress, const QString& status);

private:
    // Возвращает случайную фразу с учётом времени суток и вероятностей.
    [[nodiscard]] QString getRandomPhrase() const;

    QLabel*       m_logoLabel    {nullptr};  // "Naleystogramm"
    QLabel*       m_versionLabel {nullptr};  // "v0.2.1 ..."
    QLabel*       m_phraseLabel  {nullptr};  // забавная фраза
    QLabel*       m_statusLabel  {nullptr};  // текущий шаг загрузки
    QProgressBar* m_progress     {nullptr};  // полоса прогресса
};
