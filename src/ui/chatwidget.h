#pragma once
#include <QWidget>
#include <QDateTime>
#include <QUuid>
#include <QPixmap>
#include <QMap>
#include <QList>
#include "../core/types.h"

class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class QScrollArea;
class QVBoxLayout;
#ifdef HAVE_QT_MULTIMEDIA
class QMediaPlayer;
class QAudioOutput;
#endif
class QTimer;
class AudioRecorder;
class VoiceWaveform;

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget* parent = nullptr);

    void openConversation(const QString& peerName, bool isOnline);
    void loadHistory(const QList<Message>& messages);
    void prependHistory(const QList<Message>& messages);
    void appendMessage(const QString& text, bool outgoing, const QDateTime& ts,
                       const QString& msgId = {});
    void appendVoiceMessage(bool outgoing, int durationMs, const QDateTime& ts,
                            const QString& filePath = {});
    void setPeerStatus(const QString& status);
    void showPlaceholder();

    [[nodiscard]] int historyOffset() const { return m_historyOffset; }
    void setHistoryOffset(int v) { m_historyOffset = v; }

    void setPeerUuid(const QUuid& uuid);
    void setPeerName(const QString& name);
    void setAvatar(const QPixmap& pixmap);
    void setEnterSends(bool on);

signals:
    void sendMessage(const QString& text);
    void sendFileRequested();
    void openProfileRequested(QUuid uuid);
    void sendVoiceRequested(const QString& filePath, int durationMs);
    void loadMoreRequested();
    void callRequested(QUuid peerUuid);
    void typingStarted();
    void typingStopped();

public slots:
    void showTypingIndicator(const QString& peerName);
    void hideTypingIndicator();
    void markDelivered(const QString& msgId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onSendClicked();
    void onMicClicked();
    void onRecordingDone(const QString& filePath, int durationMs);
    void onLevelChanged(float level);

private:
    void setupUi();
    QWidget* makeBubble(const QString& text, bool outgoing, const QDateTime& ts,
                        const QString& msgId = {});
    QWidget* makeVoiceBubble(bool outgoing, int durationMs, const QDateTime& ts,
                             const QString& filePath = {});

    // Search
    void toggleSearchBar();
    void doSearch(const QString& query);
    void navigateSearch(int delta);
    void clearSearchHighlights();
    QPixmap makeBubbleAvatar(int sz) const;

    QUuid        m_peerUuid;
    int          m_historyOffset{0};
    bool         m_loadingMore{false};

    // Header
    QWidget*     m_header{nullptr};
    QLabel*      m_peerAvatar{nullptr};
    QLabel*      m_peerName{nullptr};
    QLabel*      m_peerStatus{nullptr};
    QPushButton* m_callBtn{nullptr};
    QPushButton* m_searchBtn{nullptr};
    QPushButton* m_menuBtn{nullptr};

    // Search bar (below header, collapsible)
    QWidget*     m_searchBar{nullptr};
    QLineEdit*   m_searchInput{nullptr};
    QLabel*      m_searchCount{nullptr};
    QPushButton* m_searchPrev{nullptr};
    QPushButton* m_searchNext{nullptr};

    // Messages area
    QScrollArea* m_scrollArea{nullptr};
    QWidget*     m_msgContainer{nullptr};
    QVBoxLayout* m_msgLayout{nullptr};

    // Input
    QWidget*     m_inputBar{nullptr};
    QTextEdit*   m_input{nullptr};
    QPushButton* m_sendBtn{nullptr};
    QPushButton* m_micBtn{nullptr};
    QPushButton* m_attachBtn{nullptr};
    QPushButton* m_emojiBtn{nullptr};
    QLabel*      m_recIndicator{nullptr};

    // Placeholder
    QWidget*     m_placeholder{nullptr};

    // Peer avatar pixmap (for inline bubbles)
    QPixmap      m_peerAvatarPix;

    // Recording
    AudioRecorder* m_recorder{nullptr};
    QTimer*        m_recSecTimer{nullptr};
    int            m_recSeconds{0};

#ifdef HAVE_QT_MULTIMEDIA
    QMediaPlayer*  m_player{nullptr};
    QAudioOutput*  m_audioOutput{nullptr};
    QPushButton*   m_activePlayBtn{nullptr};
    VoiceWaveform* m_activeWaveform{nullptr};
#endif

    QLabel*  m_typingLabel{nullptr};
    QTimer*  m_typingOutTimer{nullptr};
    bool     m_typingActive{false};

    QMap<QString, QLabel*> m_deliveryIcons;

    // Search state
    QList<QWidget*> m_searchHits;
    int             m_searchIdx{-1};
};
