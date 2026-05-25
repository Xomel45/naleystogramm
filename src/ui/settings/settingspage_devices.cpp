#include "settingspage_devices.h"
#include "settingshelpers.h"
#include "../dialogs/devicepairingdialog.h"
#include "../dialogs/devicelinkdialog.h"
#include <QPushButton>
#include <QHBoxLayout>
#include <QDialog>

SettingsDevicesPage::SettingsDevicesPage(QWidget* parent) : SettingsPageBase(parent) {
    m_lay->addWidget(spHint(
        tr("Link multiple devices to one account. "
           "The primary device holds all encrypted sessions; "
           "secondary devices relay through it.")));
    m_lay->addSpacing(8);

    auto* btnRow = new QHBoxLayout();

    auto* primaryBtn = new QPushButton(tr("This is primary device"));
    primaryBtn->setObjectName("dlgCancelBtn");
    connect(primaryBtn, &QPushButton::clicked, this, [this]() {
        DevicePairingDialog dlg(this);
        (void)dlg.exec();
    });

    auto* secondaryBtn = new QPushButton(tr("Link to primary device"));
    secondaryBtn->setObjectName("dlgCancelBtn");
    connect(secondaryBtn, &QPushButton::clicked, this, [this]() {
        DeviceLinkDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted)
            emit connectToDeviceRequested(
                dlg.host(), static_cast<quint16>(dlg.port()), dlg.code());
    });

    btnRow->addWidget(primaryBtn);
    btnRow->addWidget(secondaryBtn);
    btnRow->addStretch();
    m_lay->addLayout(btnRow);
    m_lay->addStretch();
}
