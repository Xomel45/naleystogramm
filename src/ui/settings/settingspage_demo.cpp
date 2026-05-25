#include "settingspage_demo.h"
#include "settingshelpers.h"
#include "../thememanager.h"
#include "../../core/demomode.h"
#include <QPushButton>
#include <QHBoxLayout>

SettingsDemoPage::SettingsDemoPage(QWidget* parent) : SettingsPageBase(parent) {
    auto* demoRow = new QHBoxLayout();
    auto* toggle = new QPushButton();
    toggle->setObjectName("demoToggleBtn");
    toggle->setCheckable(true);
    toggle->setChecked(DemoMode::instance().enabled());
    toggle->setText(DemoMode::instance().enabled()
        ? tr("Demo mode enabled") : tr("Enable demo mode"));

    connect(toggle, &QPushButton::clicked, this, [toggle](bool checked) {
        DemoMode::instance().setEnabled(checked);
        toggle->setText(checked ? tr("Demo mode enabled") : tr("Enable demo mode"));
    });
    connect(&DemoMode::instance(), &DemoMode::toggled, toggle, [toggle](bool on) {
        toggle->setChecked(on);
        toggle->setText(on ? tr("Demo mode enabled") : tr("Enable demo mode"));
    });

    demoRow->addWidget(toggle);
    demoRow->addStretch();
    m_lay->addLayout(demoRow);
    m_lay->addWidget(spHint(
        tr("Hides your real data in UI.\n"
           "Name -> User-0000  |  UUID -> 00000...  |  IP -> 0.0.0.0\n"
           "The other party still sees your real data.")));
    m_lay->addStretch();
}
