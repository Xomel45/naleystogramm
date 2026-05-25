#pragma once
#include <QWidget>
#include "../core/types.h"

class QLabel;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;

// ── UpdateBanner ───────────────────────────────────────────────────────────
// Плашка обновления внизу списка чатов.
// Клик по плашке запускает скачивание; по завершении — вызывает пакетный менеджер и
// перезапускает приложение.

class UpdateBanner : public QWidget {
    Q_OBJECT
public:
    explicit UpdateBanner(QWidget* parent = nullptr);
    ~UpdateBanner() override;

    void showUpdate(const UpdateInfo& info);
    void hide();

protected:
    void mousePressEvent(QMouseEvent* ev) override;

private slots:
    void startDownload();
    void onProgress(qint64 received, qint64 total);
    void onDownloadFinished();
    void installPackage(const QString& filePath);

private:
    enum class State { Available, Downloading, Installing, Error };
    void setState(State s, const QString& extra = {});
    void restartApp(const QString& newExe = {});

    QLabel*              m_iconLabel     {nullptr};
    QLabel*              m_textLabel     {nullptr};
    QLabel*              m_progressLabel {nullptr};
    QPushButton*         m_closeBtn      {nullptr};

    UpdateInfo           m_info;
    State                m_state         {State::Available};
    QNetworkAccessManager* m_nam         {nullptr};
    QNetworkReply*       m_reply         {nullptr};
    QString              m_downloadPath;
};
