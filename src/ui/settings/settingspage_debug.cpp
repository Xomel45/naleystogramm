#include "settingspage_debug.h"
#include "settingshelpers.h"
#include "../logpanel.h"

SettingsDebugPage::SettingsDebugPage(QWidget* parent) : SettingsPageBase(parent) {
    m_lay->addWidget(spHint(
        tr("Log of network events and errors. Enable verbose mode for more details.")));
    m_lay->addSpacing(6);

    m_logPanel = new LogPanel();
    m_logPanel->setMinimumHeight(200);
    m_lay->addWidget(m_logPanel, 1);
    connect(m_logPanel, &LogPanel::verboseChanged, this, &SettingsDebugPage::verboseLoggingChanged);

    m_lay->addStretch();
}
