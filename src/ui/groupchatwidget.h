#pragma once
#include <QWidget>
#include <QList>
#include <QString>
#include "../core/types.h"

class QLabel;
class QTextEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QLineEdit;

class GroupChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit GroupChatWidget(QWidget* parent = nullptr);

    // Открыть чат группы (сброс истории)
    void openGroup(const Group& g);

    // Загрузить историю из БД
    void loadHistory(const QList<GroupMessage>& messages);

    // Добавить одно сообщение
    void appendMessage(const GroupMessage& msg);

    // Добавить системное уведомление (join/leave)
    void appendSystemMsg(const QString& text);

    // Переключить возможность ввода (false — канал, мы не admin)
    void setCanSend(bool canSend);

    // Обновить статус подключения в шапке
    void setConnected(bool connected);

    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    void sendMessage(QString text);
    void leaveRequested();
    void membersRequested();

private slots:
    void onSendClicked();

private:
    void setupUi();
    void scrollToBottom();
    QWidget* makeBubble(const GroupMessage& msg);

    Group       m_group;
    bool        m_canSend{true};

    // UI elements
    QLabel*     m_nameLabel{nullptr};
    QLabel*     m_statusLabel{nullptr};
    QPushButton* m_membersBtn{nullptr};
    QPushButton* m_leaveBtn{nullptr};
    QScrollArea* m_scrollArea{nullptr};
    QVBoxLayout* m_msgLayout{nullptr};
    QWidget*     m_msgContainer{nullptr};
    QTextEdit*   m_input{nullptr};
    QPushButton* m_sendBtn{nullptr};
    QWidget*     m_inputBar{nullptr};
};
