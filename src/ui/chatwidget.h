#pragma once
#include <QWidget>
#include <QDateTime>
#include <QUuid>
#include <QPixmap>
#include "../core/storage.h"

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

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget* parent = nullptr);

    void openConversation(const QString& peerName, bool isOnline);
    void loadHistory(const QList<Message>& messages);
    // Вставить старые сообщения в начало (lazy loading при прокрутке вверх)
    void prependHistory(const QList<Message>& messages);
    void appendMessage(const QString& text, bool outgoing, const QDateTime& ts);
    // Добавить голосовое сообщение в чат (filePath — путь к WAV, пустой для входящих при загрузке)
    void appendVoiceMessage(bool outgoing, int durationMs, const QDateTime& ts,
                            const QString& filePath = {});
    void setPeerStatus(const QString& status);
    void showPlaceholder();

    // Смещение в базе (сколько сообщений уже загружено с конца) — для lazy loading
    [[nodiscard]] int historyOffset() const { return m_historyOffset; }
    void setHistoryOffset(int v) { m_historyOffset = v; }

    // Сохраняет UUID активного пира (нужен для сигнала openProfileRequested)
    void setPeerUuid(const QUuid& uuid);

    // Обновить имя пира в шапке чата (без перезагрузки истории)
    void setPeerName(const QString& name);

    // Устанавливает аватар в шапке чата с эллиптической маской.
    // Пустой пиксмап → показать букву вместо изображения.
    void setAvatar(const QPixmap& pixmap);

signals:
    void sendMessage(const QString& text);
    void sendFileRequested();
    void openProfileRequested(QUuid uuid);   // клик по аватару пира в шапке
    // Готово голосовое сообщение для отправки
    void sendVoiceRequested(const QString& filePath, int durationMs);
    // Пользователь прокрутил до верха — запросить следующую порцию истории
    void loadMoreRequested();
    // Пользователь нажал кнопку голосового звонка
    void callRequested(QUuid peerUuid);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onSendClicked();
    void onMicClicked();
    void onRecordingDone(const QString& filePath, int durationMs);
    void onLevelChanged(float level);

private:
    void setupUi();
    QWidget* makeBubble(const QString& text, bool outgoing, const QDateTime& ts);
    // Создать пузырь для голосового сообщения с кнопкой воспроизведения
    QWidget* makeVoiceBubble(bool outgoing, int durationMs, const QDateTime& ts,
                             const QString& filePath = {});

    QUuid        m_peerUuid;              // UUID активного пира
    int          m_historyOffset{0};      // сколько сообщений уже загружено (offset в DB)
    bool         m_loadingMore{false};    // защита от двойного emit loadMoreRequested

    // Header
    QWidget*     m_header{nullptr};
    QLabel*      m_peerAvatar{nullptr};   // аватар с буквой или изображением
    QLabel*      m_peerName{nullptr};
    QLabel*      m_peerStatus{nullptr};
    QPushButton* m_fileBtn{nullptr};
    QPushButton* m_callBtn{nullptr};         // кнопка голосового звонка

    // Messages area
    QScrollArea* m_scrollArea{nullptr};
    QWidget*     m_msgContainer{nullptr};
    QVBoxLayout* m_msgLayout{nullptr};

    // Input
    QWidget*     m_inputBar{nullptr};
    QTextEdit*   m_input{nullptr};
    QPushButton* m_sendBtn{nullptr};
    QPushButton* m_micBtn{nullptr};        // кнопка записи голосового
    QLabel*      m_recIndicator{nullptr};  // "🔴 0:03" — показывается при записи

    // Placeholder
    QWidget*     m_placeholder{nullptr};

    // Запись голосовых
    AudioRecorder* m_recorder{nullptr};
    QTimer*        m_recSecTimer{nullptr};
    int            m_recSeconds{0};

    // Воспроизведение голосовых (один плеер на виджет)
#ifdef HAVE_QT_MULTIMEDIA
    QMediaPlayer*  m_player{nullptr};
    QAudioOutput*  m_audioOutput{nullptr};
    QPushButton*   m_activePlayBtn{nullptr};  // текущая активная кнопка ▶/⏸
#endif
};
