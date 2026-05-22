#pragma once
#include <QDialog>

class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;

// ── DevicePairingDialog ───────────────────────────────────────────────────────
// Открывается на ГЛАВНОМ устройстве.
// Показывает сгенерированный 6-значный код, обратный отсчёт (60 сек),
// кнопку «Новый код» и список уже привязанных устройств.

class DevicePairingDialog : public QDialog {
    Q_OBJECT
public:
    explicit DevicePairingDialog(QWidget* parent = nullptr);

private slots:
    void onNewCode();
    void onTick();

private:
    void refreshDeviceList();

    QLabel*      m_codeLabel      {nullptr};
    QLabel*      m_countdownLabel {nullptr};
    QPushButton* m_newCodeBtn     {nullptr};
    QVBoxLayout* m_devicesLayout  {nullptr};
    QLabel*      m_noDevicesLabel {nullptr};
    QTimer*      m_timer          {nullptr};
    int          m_secondsLeft    {0};
};
