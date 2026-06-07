#include "updatebanner.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>
#include <QStandardPaths>

UpdateBanner::UpdateBanner(QWidget* parent) : QWidget(parent) {
    setObjectName("updateBanner");
    setFixedHeight(44);
    setCursor(Qt::PointingHandCursor);
    QWidget::hide();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 8, 0);
    layout->setSpacing(8);

    m_iconLabel = new QLabel("🔄");
    m_iconLabel->setObjectName("updateBannerIcon");
    m_iconLabel->setFixedWidth(20);
    m_iconLabel->setAlignment(Qt::AlignCenter);

    m_textLabel = new QLabel();
    m_textLabel->setObjectName("updateBannerText");

    m_progressLabel = new QLabel();
    m_progressLabel->setObjectName("updateBannerProgress");
    m_progressLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_progressLabel->setOpenExternalLinks(true);
    m_progressLabel->hide();

    m_closeBtn = new QPushButton("✕");
    m_closeBtn->setObjectName("iconBtn");
    m_closeBtn->setFixedSize(22, 22);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        if (m_reply) { m_reply->abort(); m_reply = nullptr; }
        QWidget::hide();
    });

    layout->addWidget(m_iconLabel);
    layout->addWidget(m_textLabel, 1);
    layout->addWidget(m_progressLabel);
    layout->addWidget(m_closeBtn);
}

UpdateBanner::~UpdateBanner() {
    if (m_reply) m_reply->abort();
}

// ── Публичный API ──────────────────────────────────────────────────────────

void UpdateBanner::showUpdate(const UpdateInfo& info) {
    m_info  = info;
    m_state = State::Available;
    m_iconLabel->setText("🔄");
    m_textLabel->setText(QString("Доступно обновление %1 — нажмите для загрузки").arg(QString::fromStdString(info.version)));
    m_progressLabel->hide();
    m_closeBtn->show();
    setCursor(Qt::PointingHandCursor);
    QWidget::show();
}

void UpdateBanner::hide() {
    if (m_reply) { m_reply->abort(); m_reply = nullptr; }
    QWidget::hide();
}

// ── Мышь ──────────────────────────────────────────────────────────────────

void UpdateBanner::mousePressEvent(QMouseEvent* ev) {
    if (m_state == State::Available)
        startDownload();
    QWidget::mousePressEvent(ev);
}

// ── Загрузка ──────────────────────────────────────────────────────────────

void UpdateBanner::startDownload() {
    if (m_info.downloadUrl.empty()) {
        QDesktopServices::openUrl(QUrl(QString::fromStdString(m_info.url)));
        return;
    }

    m_state = State::Downloading;
    m_iconLabel->setText("⬇");
    m_textLabel->setText(QString("Загрузка %1...").arg(QString::fromStdString(m_info.version)));
    m_progressLabel->setTextFormat(Qt::PlainText);
    m_progressLabel->setText("0.00%");
    m_progressLabel->show();
    m_closeBtn->show();
    setCursor(Qt::ArrowCursor);

    const QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_downloadPath = tmp + "/" + QString::fromStdString(m_info.assetName);

    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
        m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    }

    QNetworkRequest req{QUrl{QString::fromStdString(m_info.downloadUrl)}};
    req.setRawHeader("User-Agent", QByteArray("naleystogramm/") + APP_VERSION);

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &UpdateBanner::onProgress);
    connect(m_reply, &QNetworkReply::finished,         this, &UpdateBanner::onDownloadFinished);
}

void UpdateBanner::onProgress(qint64 received, qint64 total) {
    if (total <= 0) return;
    const double pct = 100.0 * received / total;
    m_progressLabel->setText(QString("%1%").arg(pct, 0, 'f', 2));
}

void UpdateBanner::onDownloadFinished() {
    if (!m_reply) return;

    if (m_reply->error() != QNetworkReply::NoError
        && m_reply->error() != QNetworkReply::OperationCanceledError) {
        m_reply->deleteLater();
        m_reply = nullptr;
        m_state = State::Error;
        m_iconLabel->setText("⚠");
        m_textLabel->setText("Ошибка загрузки");
        const QString link = QString("<a href=\"%1\">открыть страницу</a>").arg(QString::fromStdString(m_info.url));
        m_progressLabel->setTextFormat(Qt::RichText);
        m_progressLabel->setText(link);
        m_progressLabel->show();
        return;
    }

    QFile file(m_downloadPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(m_reply->readAll());
        file.close();
    }
    m_reply->deleteLater();
    m_reply = nullptr;

    m_progressLabel->setText("100.00%");
    installPackage(m_downloadPath);
}

// ── Установка ─────────────────────────────────────────────────────────────

void UpdateBanner::installPackage(const QString& filePath) {
    m_state = State::Installing;
    m_iconLabel->setText("⚙");
    m_textLabel->setText(QString("Устанавливаю %1...").arg(QString::fromStdString(m_info.version)));
    m_progressLabel->hide();
    m_closeBtn->hide();
    setCursor(Qt::WaitCursor);

    // AppImage — просто заменяем себя и перезапускаем, без pkexec
    if (filePath.endsWith(".AppImage")) {
        const QString appimageEnv = qEnvironmentVariable("APPIMAGE");
        const QString dest = appimageEnv.isEmpty()
            ? QCoreApplication::applicationFilePath()
            : appimageEnv;
        QFile::remove(dest);
        if (QFile::copy(filePath, dest)) {
            QFile::setPermissions(dest,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                QFileDevice::ReadOther  | QFileDevice::ExeOther);
        }
        restartApp(dest);
        return;
    }

    QString program = "pkexec";
    QStringList args;
    if (filePath.endsWith(".pkg.tar.zst")) {
        args = {"pacman", "-U", "--noconfirm", filePath};
    } else if (filePath.endsWith(".deb")) {
        args = {"dpkg", "-i", filePath};
    } else if (filePath.endsWith(".rpm")) {
        if (QFile::exists("/usr/bin/dnf") || QFile::exists("/bin/dnf"))
            args = {"dnf", "install", "-y", filePath};
        else
            args = {"rpm", "-i", filePath};
    } else {
        // Неизвестный формат — открываем страницу
        QDesktopServices::openUrl(QUrl(QString::fromStdString(m_info.url)));
        QWidget::hide();
        return;
    }

    auto* proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
                proc->deleteLater();
                if (exitCode == 0) {
                    restartApp();
                } else {
                    m_state = State::Error;
                    m_iconLabel->setText("⚠");
                    m_textLabel->setText("Ошибка установки");
                    m_progressLabel->setTextFormat(Qt::RichText);
                    m_progressLabel->setText(
                        QString("<a href=\"%1\">открыть страницу</a>").arg(QString::fromStdString(m_info.url)));
                    m_progressLabel->show();
                    m_closeBtn->show();
                    setCursor(Qt::PointingHandCursor);
                }
            });
    proc->start(program, args);
}

// ── Перезапуск ────────────────────────────────────────────────────────────

void UpdateBanner::restartApp(const QString& newExe) {
    const QString exe  = newExe.isEmpty() ? QCoreApplication::applicationFilePath() : newExe;
    QStringList   args = QCoreApplication::arguments();
    args.removeFirst();
    QProcess::startDetached(exe, args);
    QCoreApplication::quit();
}
