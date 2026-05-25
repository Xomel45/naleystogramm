#include "settingspage_updates.h"
#include "settingshelpers.h"
#include "../toggleswitch.h"
#include "../../core/sessionmanager.h"
#include "../../core/updatechecker.h"
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>

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

    auto* checker = new UpdateChecker(this);

    connect(checkBtn, &QPushButton::clicked, this, [this, checkBtn, checker]() {
        checkBtn->setEnabled(false);
        checkBtn->setText(tr("Checking..."));
        m_updateStatusLabel->setText("");
        checker->checkNow();
    });

    connect(checker, &UpdateChecker::updateAvailable, this, [this, checkBtn](const UpdateInfo& info) {
        checkBtn->setEnabled(true);
        checkBtn->setText(tr("Check for updates"));
        m_lastCheckedLabel->setText(tr("Checked: ") + UpdateChecker().lastChecked());
        m_updateStatusLabel->setText(tr("New version available: <b>%1</b>").arg(info.version));

        auto* openBtn = new QPushButton(tr("Open release page"));
        openBtn->setObjectName("dlgOkBtn");
        connect(openBtn, &QPushButton::clicked, this, [info]() {
            QDesktopServices::openUrl(QUrl(info.url));
        });
        qobject_cast<QVBoxLayout*>(m_updateStatusLabel->parentWidget()->layout())->addWidget(openBtn);
    });

    connect(checker, &UpdateChecker::noUpdateAvailable, this, [this, checkBtn](const QString& ver) {
        checkBtn->setEnabled(true);
        checkBtn->setText(tr("Check for updates"));
        m_lastCheckedLabel->setText(tr("Checked: ") + UpdateChecker().lastChecked());
        m_updateStatusLabel->setText(tr("Version %1 is up to date").arg(ver));
    });

    connect(checker, &UpdateChecker::checkFailed, this, [this, checkBtn](const QString& err) {
        checkBtn->setEnabled(true);
        checkBtn->setText(tr("Check for updates"));
        m_updateStatusLabel->setText(tr("Error: ") + err);
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

void SettingsUpdatesPage::reload() {
    if (m_autoCheckToggle)
        m_autoCheckToggle->setChecked(SessionManager::instance().autoCheckUpdates());
}
