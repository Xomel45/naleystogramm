#pragma once
#include <QWidget>
#include <QDateTime>
#include "../core/storage.h"

class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class QScrollArea;
class QVBoxLayout;

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget* parent = nullptr);

    void openConversation(const QString& peerName, bool isOnline);
    void loadHistory(const QList<Message>& messages);
    void appendMessage(const QString& text, bool outgoing, const QDateTime& ts);
    void setPeerStatus(const QString& status);
    void showPlaceholder();

signals:
    void sendMessage(const QString& text);
    void sendFileRequested();

private slots:
    void onSendClicked();

private:
    void setupUi();
    QWidget* makeBubble(const QString& text, bool outgoing, const QDateTime& ts);

    // Header
    QWidget*     m_header{nullptr};
    QLabel*      m_peerAvatar{nullptr};   // аватар с буквой
    QLabel*      m_peerName{nullptr};
    QLabel*      m_peerStatus{nullptr};
    QPushButton* m_fileBtn{nullptr};

    // Messages area
    QScrollArea* m_scrollArea{nullptr};
    QWidget*     m_msgContainer{nullptr};
    QVBoxLayout* m_msgLayout{nullptr};

    // Input
    QWidget*     m_inputBar{nullptr};
    QTextEdit*   m_input{nullptr};
    QPushButton* m_sendBtn{nullptr};

    // Placeholder
    QWidget*     m_placeholder{nullptr};
};
