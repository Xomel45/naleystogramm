#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;
class QLabel;
class QPushButton;

class JoinGroupDialog : public QDialog {
    Q_OBJECT
public:
    explicit JoinGroupDialog(const QString& defaultUsername, QWidget* parent = nullptr);

    [[nodiscard]] QString serverUrl() const;
    [[nodiscard]] QString username()  const;

private slots:
    void validate();

private:
    QLineEdit* m_urlEdit{nullptr};
    QLineEdit* m_usernameEdit{nullptr};
    QLabel*    m_errorLabel{nullptr};
    QPushButton* m_joinBtn{nullptr};
};
