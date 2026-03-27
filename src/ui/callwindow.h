#pragma once
#include <QDialog>
#include <QElapsedTimer>

class QLabel;
class QPushButton;
class QProgressBar;
class QTimer;

// ── CallWindow ────────────────────────────────────────────────────────────────
// Плавающее окно управления голосовым звонком (320×200).
//
// Состояния:
//   Calling — исходящий, ожидаем ответа
//   Ringing — входящий, пользователь выбирает принять/отклонить
//   InCall  — разговор идёт, показывает таймер + уровень звука
class CallWindow : public QDialog {
    Q_OBJECT
public:
    enum class State { Calling, Ringing, InCall };

    explicit CallWindow(QWidget* parent = nullptr);
    ~CallWindow() override;

    // Установить имя собеседника в заголовке окна
    void setPeerName(const QString& name);
    // Переключить отображаемое состояние
    void setState(State s);
    // Обновить индикатор уровня звука (0.0–1.0), вызывается из MediaEngine::audioLevelChanged
    void setAudioLevel(float level);

signals:
    void muteToggled(bool muted);    // кнопка 🔇 нажата
    void hangupClicked();            // кнопка 📵 Завершить
    void acceptClicked();            // кнопка ✅ Принять (только при Ringing)
    void rejectClicked();            // кнопка ❌ Отклонить (только при Ringing)

private slots:
    void onDurationTick();           // обновляем счётчик времени разговора

private:
    void setupUi();
    void applyState();

    State m_state {State::Calling};

    QLabel*      m_peerLabel    {nullptr};  // имя собеседника
    QLabel*      m_statusLabel  {nullptr};  // "Исходящий звонок...", "0:42" и т.п.
    QProgressBar* m_levelBar    {nullptr};  // индикатор уровня звука
    QPushButton* m_muteBtn      {nullptr};
    QPushButton* m_hangupBtn    {nullptr};
    QPushButton* m_acceptBtn    {nullptr};  // видима только при Ringing
    QPushButton* m_rejectBtn    {nullptr};  // видима только при Ringing

    QTimer*       m_durationTimer {nullptr};
    QElapsedTimer m_callTimer;
    bool          m_muted         {false};
};
