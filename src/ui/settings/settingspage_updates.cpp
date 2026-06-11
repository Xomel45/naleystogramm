#include "settingspage_updates.h"
#include "settingshelpers.h"
#include "../toggleswitch.h"
#include "../../core/storage/sessionmanager.h"
#include "../../core/diag/updatechecker.h"
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QMetaObject>

SettingsUpdatesPage::SettingsUpdatesPage(QWidget* parent) : SettingsPageBase(parent) {
    auto* versionRow = new QHBoxLayout();
    auto* verLabel = new QLabel(tr("Current version"));
    verLabel->setObjectName("settingsFieldLabel");
    auto* verVal = new QLabel(UpdateChecker::kCurrentVersion);
    verVal->setObjectName("settingsHint");
    versionRow->addWidget(verLabel);
    versionRow->addStretch();
    versionRow->addWidget(verVal);
    m_lay->addLayout(versionRow);
    m_lay->addSpacing(8);

    m_lastCheckedLabel = new QLabel();
    m_lastCheckedLabel->setObjectName("settingsHint");

    m_updateStatusLabel = new QLabel(tr("Press button to check"));
    m_updateStatusLabel->setObjectName("settingsHint");
    m_updateStatusLabel->setWordWrap(true);

    auto* checkBtn = new QPushButton(tr("Check for updates"));
    checkBtn->setObjectName("dlgCancelBtn");

    m_checker = new UpdateChecker();

    m_checker->subscribeUpdateAvailable([this, checkBtn](const UpdateInfo& info) {
        const QString ver  = QString::fromStdString(info.version);
        const QString url  = QString::fromStdString(info.url);
        const QString last = QString::fromStdString(m_checker->lastChecked());
        QMetaObject::invokeMethod(this, [this, checkBtn, ver, url, last]() {
            checkBtn->setEnabled(true);
            checkBtn->setText(tr("Check for updates"));
            m_lastCheckedLabel->setText(tr("Checked: ") + last);
            m_updateStatusLabel->setText(tr("New version available: <b>%1</b>").arg(ver));

            auto* openBtn = new QPushButton(tr("Open release page"));
            openBtn->setObjectName("dlgOkBtn");
            connect(openBtn, &QPushButton::clicked, this, [url]() {
                QDesktopServices::openUrl(QUrl(url));
            });
            qobject_cast<QVBoxLayout*>(m_updateStatusLabel->parentWidget()->layout())->addWidget(openBtn);
        }, Qt::QueuedConnection);
    });

    m_checker->subscribeNoUpdate([this, checkBtn](const std::string& ver) {
        const QString qver = QString::fromStdString(ver);
        const QString last = QString::fromStdString(m_checker->lastChecked());
        QMetaObject::invokeMethod(this, [this, checkBtn, qver, last]() {
            checkBtn->setEnabled(true);
            checkBtn->setText(tr("Check for updates"));
            m_lastCheckedLabel->setText(tr("Checked: ") + last);
            m_updateStatusLabel->setText(tr("Version %1 is up to date").arg(qver));
        }, Qt::QueuedConnection);
    });

    m_checker->subscribeCheckFailed([this, checkBtn](const std::string& err) {
        const QString qerr = QString::fromStdString(err);
        QMetaObject::invokeMethod(this, [this, checkBtn, qerr]() {
            checkBtn->setEnabled(true);
            checkBtn->setText(tr("Check for updates"));
            m_updateStatusLabel->setText(tr("Error: ") + qerr);
        }, Qt::QueuedConnection);
    });

    connect(checkBtn, &QPushButton::clicked, this, [this, checkBtn]() {
        checkBtn->setEnabled(false);
        checkBtn->setText(tr("Checking..."));
        m_updateStatusLabel->setText("");
        m_checker->checkNow();
    });

    m_lay->addWidget(m_lastCheckedLabel);
    m_lay->addWidget(checkBtn);
    m_lay->addWidget(m_updateStatusLabel);
    m_lay->addSpacing(16);

    auto* autoRow = new QHBoxLayout();
    auto* autoLabel = new QLabel(tr("Авто-обновления"));
    autoLabel->setObjectName("settingsFieldLabel");
    m_autoCheckToggle = new ToggleSwitch();
    m_autoCheckToggle->setChecked(SessionManager::instance().autoCheckUpdates());
    connect(m_autoCheckToggle, &QAbstractButton::toggled, this, [](bool on) {
        SessionManager::instance().setAutoCheckUpdates(on);
    });
    autoRow->addWidget(autoLabel);
    autoRow->addStretch();
    autoRow->addWidget(m_autoCheckToggle);
    m_lay->addLayout(autoRow);
    m_lay->addWidget(spHint(tr("Проверять обновления при запуске (раз в 6 часов)")));
    m_lay->addStretch();
}

SettingsUpdatesPage::~SettingsUpdatesPage() {
    delete m_checker;
}

void SettingsUpdatesPage::reload() {
    if (m_autoCheckToggle)
        m_autoCheckToggle->setChecked(SessionManager::instance().autoCheckUpdates());
}
