#pragma once
#include <QDialog>

class QProgressBar;
class QLabel;
class QGraphicsOpacityEffect;

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
    [[nodiscard]] QString getRandomPhrase() const;

    QLabel*       m_logoLabel    {nullptr};
    QLabel*       m_versionLabel {nullptr};
    QLabel*       m_phraseLabel  {nullptr};
    QLabel*       m_statusLabel  {nullptr};
    QLabel*       m_creditsLabel {nullptr};  // "Made by Xomelz & Claude" — появляется постепенно
    QProgressBar* m_progress     {nullptr};

    QGraphicsOpacityEffect* m_creditsOpacity {nullptr};
};
