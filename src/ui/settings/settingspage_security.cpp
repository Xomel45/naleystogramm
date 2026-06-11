#include "settingspage_security.h"
#include "settingshelpers.h"
#include "../../core/storage/sessionmanager.h"
#include <QPushButton>
#include <QHBoxLayout>

SettingsSecurityPage::SettingsSecurityPage(QWidget* parent) : SettingsPageBase(parent) {
    auto* shellRow = new QHBoxLayout();
    m_shellToggle = new QPushButton();
    m_shellToggle->setObjectName("demoToggleBtn");
    m_shellToggle->setCheckable(true);
    m_shellToggle->setChecked(SessionManager::instance().remoteShellEnabled());
    m_shellToggle->setText(SessionManager::instance().remoteShellEnabled()
        ? tr("Remote shell allowed") : tr("Remote shell blocked"));

    connect(m_shellToggle, &QPushButton::clicked, this, [this](bool checked) {
        SessionManager::instance().setRemoteShellEnabled(checked);
        m_shellToggle->setText(checked ? tr("Remote shell allowed") : tr("Remote shell blocked"));
    });

    shellRow->addWidget(m_shellToggle);
    shellRow->addStretch();
    m_lay->addLayout(shellRow);
    m_lay->addWidget(spHint(
        tr("Разрешить контактам запрашивать доступ к терминалу на вашем устройстве.\n"
           "При отключении все входящие запросы удалённого шелла отклоняются автоматически.")));
    m_lay->addStretch();
}

void SettingsSecurityPage::reload() {
    auto& sm = SessionManager::instance();
    m_shellToggle->setChecked(sm.remoteShellEnabled());
    m_shellToggle->setText(sm.remoteShellEnabled()
        ? tr("Remote shell allowed") : tr("Remote shell blocked"));
}
