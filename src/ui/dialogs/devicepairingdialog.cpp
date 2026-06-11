#include "devicepairingdialog.h"
#include "../../core/identity/device_pairing.h"
#include "../../core/net/network.h"
#include "../../core/storage/sessionmanager.h"
#include "../qrcodewidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QTimer>

// ── DevicePairingDialog ───────────────────────────────────────────────────────

DevicePairingDialog::DevicePairingDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Привязать устройство"));
    setFixedWidth(380);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("Привязка устройства"));
    title->setObjectName("dlgTitle");
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        tr("Отсканируйте QR-код на вторичном устройстве или введите код вручную."));
    subtitle->setObjectName("dlgSubtitle");
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    layout->addSpacing(8);

    // ── QR-код ───────────────────────────────────────────────────────────────
    m_qrWidget = new QrCodeWidget(200, this);
    auto* qrRow = new QHBoxLayout();
    qrRow->addStretch();
    qrRow->addWidget(m_qrWidget);
    qrRow->addStretch();
    layout->addLayout(qrRow);

    layout->addSpacing(4);

    // ── Текстовый код ────────────────────────────────────────────────────────
    m_codeLabel = new QLabel();
    m_codeLabel->setObjectName("pairingCode");
    m_codeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_codeLabel);

    m_countdownLabel = new QLabel();
    m_countdownLabel->setObjectName("dlgSubtitle");
    m_countdownLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_countdownLabel);

    m_newCodeBtn = new QPushButton(tr("Новый код"));
    m_newCodeBtn->setObjectName("dlgCancelBtn");
    connect(m_newCodeBtn, &QPushButton::clicked, this, &DevicePairingDialog::onNewCode);

    auto* codeRow = new QHBoxLayout();
    codeRow->addStretch();
    codeRow->addWidget(m_newCodeBtn);
    codeRow->addStretch();
    layout->addLayout(codeRow);

    layout->addSpacing(8);

    // ── Разделитель + список устройств ───────────────────────────────────────
    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("settingsSeparator");
    layout->addWidget(sep);

    auto* devTitle = new QLabel(tr("Привязанные устройства"));
    devTitle->setObjectName("dlgSubtitle");
    layout->addWidget(devTitle);

    m_devicesLayout = new QVBoxLayout();
    m_devicesLayout->setSpacing(4);
    layout->addLayout(m_devicesLayout);

    m_noDevicesLabel = new QLabel(tr("Нет привязанных устройств"));
    m_noDevicesLabel->setObjectName("settingsHint");
    m_devicesLayout->addWidget(m_noDevicesLabel);

    layout->addSpacing(8);

    auto* closeBtn = new QPushButton(tr("Закрыть"));
    closeBtn->setObjectName("dlgOkBtn");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    // ── Таймер ───────────────────────────────────────────────────────────────
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &DevicePairingDialog::onTick);

    onNewCode();
    refreshDeviceList();
}

void DevicePairingDialog::onNewCode() {
    const QString code = QString::fromStdString(DevicePairing::generateCode());

    // Отображаем как "XXX XXX" для читаемости
    m_codeLabel->setText(code.left(3) + QStringLiteral("  ") + code.right(3));

    // QR кодирует URI вида naleys://pair?ip=…&port=…&code=…
    const QString ip  = QString::fromStdString(NetworkManager::detectLocalLanIp());
    const QString uri = QStringLiteral("naleys://pair?ip=%1&port=%2&code=%3")
                            .arg(ip)
                            .arg(NetworkManager::kDefaultPort)
                            .arg(code);
    m_qrWidget->setContent(uri);

    m_secondsLeft = DevicePairing::kCodeTtlSecs;
    m_countdownLabel->setText(tr("Действителен %1 сек").arg(m_secondsLeft));
    m_timer->start();
}

void DevicePairingDialog::onTick() {
    --m_secondsLeft;
    if (m_secondsLeft <= 0) {
        m_timer->stop();
        m_codeLabel->setText(tr("— истёк —"));
        m_countdownLabel->setText(tr("Нажмите «Новый код»"));
        m_qrWidget->clear();
        DevicePairing::clearCode();
        return;
    }
    m_countdownLabel->setText(tr("Действителен %1 сек").arg(m_secondsLeft));
}

void DevicePairingDialog::refreshDeviceList() {
    // Удаляем старые строки (кроме m_noDevicesLabel)
    QLayoutItem* item;
    while ((item = m_devicesLayout->takeAt(0)) != nullptr) {
        if (item->widget() && item->widget() != m_noDevicesLabel)
            item->widget()->deleteLater();
        delete item;
    }

    const auto devices = SessionManager::instance().linkedDevices();
    if (devices.empty()) {
        m_devicesLayout->addWidget(m_noDevicesLabel);
        m_noDevicesLabel->show();
        return;
    }

    m_noDevicesLabel->hide();
    for (const auto& dev : devices) {
        auto* row = new QHBoxLayout();

        auto* nameLbl = new QLabel(QString::fromStdString(dev.name));
        nameLbl->setObjectName("settingsFieldLabel");

        auto* roleLbl = new QLabel(dev.isPrimary ? tr("главное") : tr("вторичное"));
        roleLbl->setObjectName("settingsHint");

        auto* removeBtn = new QPushButton(tr("Отвязать"));
        removeBtn->setObjectName("dlgCancelBtn");
        const std::string uuid = dev.uuid;
        connect(removeBtn, &QPushButton::clicked, this, [this, uuid]() {
            SessionManager::instance().removeLinkedDevice(uuid);
            refreshDeviceList();
        });

        row->addWidget(nameLbl, 1);
        row->addWidget(roleLbl);
        row->addWidget(removeBtn);

        auto* rowWidget = new QWidget();
        rowWidget->setLayout(row);
        m_devicesLayout->addWidget(rowWidget);
    }
}
