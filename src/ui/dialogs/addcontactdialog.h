#pragma once
#include <QDialog>

class QTextEdit;
class QLabel;

// ── Add Contact Dialog ────────────────────────────────────────────────────

class AddContactDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddContactDialog(QWidget* parent = nullptr);
    QString connectionString() const;

private:
    QTextEdit* m_input{nullptr};
    QLabel*    m_error{nullptr};
};

// FIX: IncomingDialog вынесен в отдельный incomingdialog.h
