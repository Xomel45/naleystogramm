#include "settingspage_profile.h"
#include "settingshelpers.h"
#include "../thememanager.h"
#include "../../core/identity.h"
#include "../../core/sessionmanager.h"
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QStandardPaths>
#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QDir>
#include <QDate>

SettingsProfilePage::SettingsProfilePage(QWidget* parent) : SettingsPageBase(parent) {
    m_lay->setSpacing(0);
    auto& sm = SessionManager::instance();

    // ── Шапка ────────────────────────────────────────────────────────────────
    auto* header = new QWidget();
    header->setObjectName("accountHeader");
    auto* headerLay = new QVBoxLayout(header);
    headerLay->setContentsMargins(16, 28, 16, 20);
    headerLay->setSpacing(6);
    headerLay->setAlignment(Qt::AlignHCenter);

    auto* avatarWrap = new QWidget();
    avatarWrap->setFixedSize(96, 96);

    m_avatarLabel = new QLabel(avatarWrap);
    m_avatarLabel->setObjectName("settingsAvatar");
    m_avatarLabel->setFixedSize(90, 90);
    m_avatarLabel->move(0, 0);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setCursor(Qt::PointingHandCursor);
    m_avatarLabel->installEventFilter(this);

    m_changeAvatarBtn = new QPushButton(avatarWrap);
    m_changeAvatarBtn->setObjectName("settingsAvatarCam");
    m_changeAvatarBtn->setFixedSize(28, 28);
    m_changeAvatarBtn->move(90 - 26, 90 - 26);
    m_changeAvatarBtn->setIcon(ThemeManager::tintedIcon(":/icons/settings_photo.png"));
    m_changeAvatarBtn->setIconSize(QSize(16, 16));
    m_changeAvatarBtn->raise();
    connect(m_changeAvatarBtn, &QPushButton::clicked, this, &SettingsProfilePage::onAvatarClicked);

    m_profileNameLbl = new QLabel();
    m_profileNameLbl->setObjectName("accountName");
    m_profileNameLbl->setAlignment(Qt::AlignHCenter);

    auto* idLbl = new QLabel(QString::fromStdString(sm.uuid()).left(8) + "…");
    idLbl->setObjectName("accountId");
    idLbl->setAlignment(Qt::AlignHCenter);

    headerLay->addWidget(avatarWrap, 0, Qt::AlignHCenter);
    headerLay->addWidget(m_profileNameLbl);
    headerLay->addWidget(idLbl);
    m_lay->addWidget(header);

    // ── Разделители ───────────────────────────────────────────────────────────
    auto mkSep = [&]() {
        auto* f = new QFrame();
        f->setFrameShape(QFrame::HLine);
        f->setObjectName("settingsSeparator");
        return f;
    };
    auto mkInsetSep = [&]() {
        auto* f = new QFrame();
        f->setFrameShape(QFrame::HLine);
        f->setObjectName("settingsInsetSeparator");
        return f;
    };

    m_lay->addWidget(mkSep());

    // ── О себе ────────────────────────────────────────────────────────────────
    auto* bioSection = new QWidget();
    bioSection->setObjectName("accountSection");
    auto* bioLay = new QVBoxLayout(bioSection);
    bioLay->setContentsMargins(16, 10, 16, 10);
    bioLay->setSpacing(6);
    m_bioEdit = new QTextEdit();
    m_bioEdit->setObjectName("accountBio");
    m_bioEdit->setPlaceholderText(
        tr("Любые подробности о себе: возраст, род занятий или город.\n"
           "Пример: 25 лет, разработчик из Москвы."));
    m_bioEdit->setAcceptRichText(false);
    m_bioEdit->setFixedHeight(72);
    m_bioEdit->document()->setDocumentMargin(0);
    bioLay->addWidget(m_bioEdit);
    m_lay->addWidget(bioSection);

    m_lay->addWidget(mkSep());

    // ── Поля ──────────────────────────────────────────────────────────────────
    auto makeInfoField = [&](const QString& iconPath, const QString& label,
                              QLineEdit*& outEdit, bool readOnly = false) -> QWidget* {
        auto* row = new QWidget();
        row->setObjectName("accountInfoRow");
        row->setFixedHeight(52);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(16, 0, 16, 0);
        rl->setSpacing(12);
        auto* ico = new QLabel();
        ico->setPixmap(ThemeManager::tintedIcon(iconPath).pixmap(20, 20));
        ico->setFixedSize(24, 24);
        ico->setAlignment(Qt::AlignCenter);
        ico->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("accountInfoLabel");
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        outEdit = new QLineEdit();
        outEdit->setObjectName("accountInfoValue");
        outEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        outEdit->setReadOnly(readOnly);
        if (readOnly) {
            outEdit->setCursor(Qt::PointingHandCursor);
            outEdit->setToolTip(tr("Нажмите чтобы скопировать"));
        }
        outEdit->setFrame(false);
        outEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        rl->addWidget(ico);
        rl->addWidget(lbl);
        rl->addStretch();
        rl->addWidget(outEdit, 1);
        return row;
    };

    m_lay->addWidget(makeInfoField(":/icons/ctx_edit.png",        tr("Имя"),          m_nameEdit));
    m_lay->addWidget(mkInsetSep());
    m_lay->addWidget(makeInfoField(":/icons/settings_profile.png", tr("День рождения"), m_birthdayEdit));
    m_birthdayEdit->setPlaceholderText("дд.мм.гггг");
    m_lay->addWidget(mkInsetSep());
    m_lay->addWidget(makeInfoField(":/icons/nav_profile.png",      tr("Мой ID"),        m_uuidEdit, true));

    m_uuidEdit->installEventFilter(this);

    m_lay->addWidget(spHint(tr("   Поделитесь своим ID — другие смогут подключиться к вам")));
    m_lay->addWidget(mkSep());

    // ── Скопировать строку подключения ────────────────────────────────────────
    auto* connRow = new QPushButton();
    connRow->setObjectName("accountActionRow");
    connRow->setFlat(true);
    connRow->setCursor(Qt::PointingHandCursor);
    connRow->setFixedHeight(52);
    {
        auto* rl = new QHBoxLayout(connRow);
        rl->setContentsMargins(16, 0, 16, 0);
        rl->setSpacing(12);
        auto* ico = new QLabel();
        ico->setPixmap(ThemeManager::tintedIcon(":/icons/ctx_copy.png").pixmap(20, 20));
        ico->setFixedSize(24, 24);
        ico->setAlignment(Qt::AlignCenter);
        ico->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* lbl = new QLabel(tr("Скопировать строку подключения"));
        lbl->setObjectName("accountInfoLabel");
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        rl->addWidget(ico);
        rl->addWidget(lbl);
        rl->addStretch();
    }
    connect(connRow, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(
            QString::fromStdString(Identity::instance().connectionString(m_externalIp.toStdString(), m_externalPort)));
    });
    m_lay->addWidget(connRow);
    m_lay->addWidget(mkSep());
    m_lay->addStretch();
}

static QString isoToDisplay(const QString& iso) {
    const QDate d = QDate::fromString(iso, Qt::ISODate);
    return d.isValid() ? d.toString("dd.MM.yyyy") : iso;
}

static QString displayToIso(const QString& display) {
    const QDate d = QDate::fromString(display, "dd.MM.yyyy");
    return d.isValid() ? d.toString(Qt::ISODate) : display;
}

void SettingsProfilePage::reload() {
    auto& sm = SessionManager::instance();
    const QString name = QString::fromStdString(sm.displayName());
    m_nameEdit->setText(name);
    m_uuidEdit->setText(QString::fromStdString(sm.uuid()).left(8) + "…");
    m_bioEdit->setPlainText(QString::fromStdString(sm.bio()));
    m_birthdayEdit->setText(isoToDisplay(QString::fromStdString(sm.birthday())));
    if (m_profileNameLbl)
        m_profileNameLbl->setText(name.isEmpty() ? tr("(без имени)") : name);

    const QString avatarPath = QString::fromStdString(sm.avatarPath());
    if (!avatarPath.isEmpty() && QFile::exists(avatarPath))
        applyAvatarPixmap(avatarPath);
    else {
        m_avatarLabel->setPixmap({});
        m_avatarLabel->setText(name.isEmpty() ? "?" : name.left(1).toUpper());
    }
}

bool SettingsProfilePage::save() {
    auto& sm = SessionManager::instance();
    const QString name = m_nameEdit->text().trimmed();
    if (!name.isEmpty() && name.toStdString() != sm.displayName()) {
        Identity::instance().setDisplayName(name.toStdString());
        emit nameChanged(name);
    }
    const QString bio = m_bioEdit->toPlainText().trimmed();
    if (bio.toStdString() != sm.bio())
        sm.setBio(bio.toStdString());
    const QString newBirthday = displayToIso(m_birthdayEdit->text().trimmed());
    if (newBirthday.toStdString() != sm.birthday())
        sm.setBirthday(newBirthday.toStdString());
    return true;
}

void SettingsProfilePage::setExternalAddress(const QString& ip, quint16 port) {
    m_externalIp   = ip;
    m_externalPort = port;
}

void SettingsProfilePage::onAvatarClicked() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        tr("Выбрать аватар"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        tr("Изображения (*.png *.jpg *.jpeg)"),
        nullptr,
        QFileDialog::DontUseNativeDialog);
    if (file.isEmpty()) return;

    QPixmap src(file);
    if (src.isNull()) return;

    const QPixmap scaled = src.scaled(128, 128, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    cacheDir.mkpath(".");
    const QString savePath = cacheDir.filePath("avatar.png");
    if (!scaled.save(savePath, "PNG")) return;

    SessionManager::instance().setAvatarPath(savePath.toStdString());
    applyAvatarPixmap(savePath);
    emit avatarChanged(savePath);
}

void SettingsProfilePage::applyAvatarPixmap(const QString& path) {
    QPixmap src(path);
    if (src.isNull()) return;
    const int sz = m_avatarLabel->width();
    const QPixmap s = src.scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QPixmap rounded(sz, sz);
    rounded.fill(Qt::transparent);
    QPainter p(&rounded);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath pp;
    pp.addEllipse(0, 0, sz, sz);
    p.setClipPath(pp);
    p.drawPixmap(0, 0, s);
    m_avatarLabel->setText({});
    m_avatarLabel->setPixmap(rounded);
}

bool SettingsProfilePage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_avatarLabel && event->type() == QEvent::MouseButtonRelease) {
        onAvatarClicked();
        return true;
    }
    if (watched == m_uuidEdit && event->type() == QEvent::MouseButtonPress) {
        const QString full = QString::fromStdString(SessionManager::instance().uuid());
        QApplication::clipboard()->setText(full);
        m_uuidEdit->setPlaceholderText(tr("Скопировано!"));
        QTimer::singleShot(1500, m_uuidEdit, [this]() {
            if (m_uuidEdit) m_uuidEdit->setPlaceholderText({});
        });
        return true;
    }
    return SettingsPageBase::eventFilter(watched, event);
}
