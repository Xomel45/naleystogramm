#pragma once
#include <QDialog>

class QLineEdit;
class QLabel;

// ── DeviceLinkDialog ──────────────────────────────────────────────────────────
// Открывается на ВТОРИЧНОМ устройстве.
// Пользователь вводит IP:порт главного устройства и 6-значный код.

class DeviceLinkDialog : public QDialog {
    Q_OBJECT
public:
    explicit DeviceLinkDialog(QWidget* parent = nullptr);

    QString host() const;
    int     port() const;
    QString code() const;

private:
    QLineEdit* m_hostEdit {nullptr};
    QLineEdit* m_codeEdit {nullptr};
    QLabel*    m_error    {nullptr};
};
