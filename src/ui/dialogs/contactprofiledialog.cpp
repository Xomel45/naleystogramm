#include "contactprofiledialog.h"
#include "../../core/network.h"
#include "../../core/storage.h"
#include "../../core/updatechecker.h"
#include "../../core/versionutils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QFile>
#include <QDateTime>
#include <QFont>
#include <QJsonDocument>

// ── Вспомогательные функции ───────────────────────────────────────────────

static QLabel* infoRow(const QString& key, QLabel*& valueOut, QWidget* parent = nullptr) {
    Q_UNUSED(parent);
    auto* lbl = new QLabel(QString("<b>%1</b>").arg(key));
    lbl->setObjectName("settingsFieldLabel");
    valueOut = new QLabel("—");
    valueOut->setObjectName("settingsHint");
    valueOut->setWordWrap(true);
    return lbl;
}

static QFrame* separator() {
    auto* f = new QFrame();
    f->setFrameShape(QFrame::HLine);
    f->setObjectName("settingsSeparator");
    return f;
}

static QLabel* sectionTitle(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setObjectName("settingsPageTitle");
    return lbl;
}

// ── Конструктор ──────────────────────────────────────────────────────────

ContactProfileDialog::ContactProfileDialog(const QUuid& peerUuid,
                                           NetworkManager* network,
                                           StorageManager* storage,
                                           QWidget* parent)
    : QDialog(parent)
    , m_uuid(peerUuid)
    , m_network(network)
    , m_storage(storage)
{
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setFixedWidth(380);
    setWindowTitle(tr("Профиль контакта"));
    setAttribute(Qt::WA_DeleteOnClose);

    setupUi();
    populateData();

    // Обновляем пинг и аптайм каждые 5 секунд
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000);
    connect(m_refreshTimer, &QTimer::timeout, this, &ContactProfileDialog::refreshData);
    m_refreshTimer->start();
}

// ── Построение UI ────────────────────────────────────────────────────────

void ContactProfileDialog::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(8);

    // ── Шапка: аватар + имя + статус ─────────────────────────────────────
    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(12);

    m_avatarLabel = new QLabel();
    m_avatarLabel->setObjectName("contactAvatar");
    m_avatarLabel->setFixedSize(80, 80);
    m_avatarLabel->setAlignment(Qt::AlignCenter);

    auto* nameCol = new QVBoxLayout();
    nameCol->setSpacing(4);

    m_nameLabel = new QLabel();
    m_nameLabel->setObjectName("myNameLabel");

    m_statusLabel = new QLabel();
    m_statusLabel->setObjectName("settingsHint");

    nameCol->addStretch();
    nameCol->addWidget(m_nameLabel);
    nameCol->addWidget(m_statusLabel);
    nameCol->addStretch();

    headerRow->addWidget(m_avatarLabel);
    headerRow->addLayout(nameCol, 1);

    root->addLayout(headerRow);
    root->addWidget(separator());

    // Предупреждение о несовместимости версии — скрыто по умолчанию,
    // показывается в populateData() если пир использует более новую версию.
    m_compatWarning = new QLabel();
    m_compatWarning->setObjectName("compatWarningLabel");
    m_compatWarning->setWordWrap(true);
    m_compatWarning->setAlignment(Qt::AlignCenter);
    m_compatWarning->hide();
    root->addWidget(m_compatWarning);

    // ── Устройство ────────────────────────────────────────────────────────
    root->addWidget(sectionTitle(tr("Устройство")));
    root->addSpacing(4);

    // Тип устройства
    auto* rowDev = new QHBoxLayout();
    rowDev->addWidget(infoRow(tr("Тип:"), m_deviceType));
    rowDev->addWidget(m_deviceType, 1);
    root->addLayout(rowDev);

    auto* rowCpu = new QHBoxLayout();
    rowCpu->addWidget(infoRow(tr("CPU:"), m_cpuRow));
    rowCpu->addWidget(m_cpuRow, 1);
    root->addLayout(rowCpu);

    auto* rowRam = new QHBoxLayout();
    rowRam->addWidget(infoRow(tr("ОЗУ:"), m_ramRow));
    rowRam->addWidget(m_ramRow, 1);
    root->addLayout(rowRam);

    auto* rowOs = new QHBoxLayout();
    rowOs->addWidget(infoRow(tr("ОС:"), m_osRow));
    rowOs->addWidget(m_osRow, 1);
    root->addLayout(rowOs);

    root->addSpacing(4);
    root->addWidget(separator());

    // ── Соединение ────────────────────────────────────────────────────────
    root->addWidget(sectionTitle(tr("Соединение")));
    root->addSpacing(4);

    auto* rowPing = new QHBoxLayout();
    rowPing->addWidget(infoRow(tr("Пинг:"), m_pingRow));
    rowPing->addWidget(m_pingRow, 1);
    root->addLayout(rowPing);

    auto* rowIp = new QHBoxLayout();
    rowIp->addWidget(infoRow(tr("IP:"), m_ipRow));
    rowIp->addWidget(m_ipRow, 1);
    root->addLayout(rowIp);

    auto* rowPort = new QHBoxLayout();
    rowPort->addWidget(infoRow(tr("Порт:"), m_portRow));
    rowPort->addWidget(m_portRow, 1);
    root->addLayout(rowPort);

    auto* rowUptime = new QHBoxLayout();
    rowUptime->addWidget(infoRow(tr("Подключён:"), m_uptimeRow));
    rowUptime->addWidget(m_uptimeRow, 1);
    root->addLayout(rowUptime);

    root->addSpacing(4);
    root->addWidget(separator());

    // ── Безопасность ──────────────────────────────────────────────────────
    root->addWidget(sectionTitle(tr("Безопасность")));
    root->addSpacing(4);

    // Номер безопасности — крупным моноширинным шрифтом для лёгкой сверки
    m_safetyLabel = new QLabel(tr("—"));
    m_safetyLabel->setObjectName("safetyNumber");
    m_safetyLabel->setAlignment(Qt::AlignCenter);
    m_safetyLabel->setFont(QFont("Monospace", 11));
    m_safetyLabel->setWordWrap(true);
    root->addWidget(m_safetyLabel);

    // Подсказка что делать с этим числом
    m_safetyHint = new QLabel(
        tr("Сверьте этот код с собеседником по голосовому/видео-звонку.\n"
           "Если коды совпадают — соединение защищено."));
    m_safetyHint->setObjectName("settingsHint");
    m_safetyHint->setAlignment(Qt::AlignCenter);
    m_safetyHint->setWordWrap(true);
    root->addWidget(m_safetyHint);

    root->addSpacing(8);

    // Кнопка удалённого шелла — видна всегда, активна только когда пир онлайн
    m_shellBtn = new QPushButton(tr(">_ Удалённый шелл"));
    m_shellBtn->setEnabled(false);   // включится в populateData() если пир онлайн
    m_shellBtn->setToolTip(
        tr("Запросить удалённую шелл-сессию.\n"
           "Доступно только когда контакт онлайн."));
    connect(m_shellBtn, &QPushButton::clicked,
            this, [this]() { emit shellRequested(m_uuid); });
    root->addWidget(m_shellBtn);

    root->addStretch();
}

// ── Заполнение данными ────────────────────────────────────────────────────

void ContactProfileDialog::populateData() {
    const Contact c = m_storage->getContact(m_uuid);
    const PeerPublicInfo info = m_network->getPeerInfo(m_uuid);

    // ── Имя ──────────────────────────────────────────────────────────────
    const QString name = c.name.isEmpty() ? tr("Неизвестный") : c.name;
    m_nameLabel->setText(name);

    // ── Аватар ───────────────────────────────────────────────────────────
    const int sz = m_avatarLabel->width();
    if (!c.avatarPath.isEmpty() && QFile::exists(c.avatarPath)) {
        QPixmap src(c.avatarPath);
        if (!src.isNull()) {
            const QPixmap scaled = src.scaled(sz, sz,
                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            QPixmap rounded(sz, sz);
            rounded.fill(Qt::transparent);
            QPainter p(&rounded);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath pp;
            pp.addEllipse(0, 0, sz, sz);
            p.setClipPath(pp);
            p.drawPixmap(0, 0, scaled);
            m_avatarLabel->setPixmap(rounded);
        }
    } else {
        // Заглушка: первая буква имени
        m_avatarLabel->setPixmap({});
        m_avatarLabel->setText(name.left(1).toUpper());
    }

    // ── Статус ───────────────────────────────────────────────────────────
    const bool online = info.state == ConnectionState::Connected;
    m_statusLabel->setText(online ? tr("● Онлайн") : tr("○ Офлайн"));

    // ── Совместимость версий ──────────────────────────────────────────────
    // Если контакт создан более новой версией приложения — часть полей может быть
    // неизвестна; показываем предупреждение и скрываем потенциально некорректные данные.
    const QString currentVer = QLatin1String(UpdateChecker::kCurrentVersion);
    const bool versionMismatch = VersionUtils::isNewerThan(c.versionCreated, currentVer);
    if (versionMismatch) {
        m_compatWarning->setText(
            tr("⚠ Этот контакт использует более новую версию приложения (v%1).\n"
               "Некоторые данные могут отображаться некорректно.\n"
               "Обновите приложение для полной совместимости.")
            .arg(c.versionCreated));
        m_compatWarning->show();
    } else {
        m_compatWarning->hide();
    }

    // ── Системная информация ──────────────────────────────────────────────
    // Приоритет: живые данные из памяти (пир онлайн) → последний снимок из БД (пир офлайн)
    const QJsonObject si = !info.systemInfo.isEmpty()
        ? info.systemInfo
        : QJsonDocument::fromJson(c.systemInfoJson.toUtf8()).object();
    // При несовместимости версии показываем прочерки вместо потенциально некорректных данных
    if (versionMismatch) {
        m_deviceType->setText("—");
        m_cpuRow->setText("—");
        m_ramRow->setText("—");
        m_osRow->setText("—");
    } else {
        m_deviceType->setText(si["deviceType"].toString("—"));
        m_cpuRow->setText(si["cpuModel"].toString("—"));
        m_ramRow->setText(si["ramAmount"].toString("—"));
        m_osRow->setText(si["osName"].toString("—"));
    }

    // ── Соединение ────────────────────────────────────────────────────────
    m_ipRow->setText(info.ip.isEmpty() ? c.ip : info.ip);
    m_portRow->setText(info.serverPort > 0
        ? QString::number(info.serverPort)
        : (c.port > 0 ? QString::number(c.port) : "—"));

    refreshData();   // обновляем динамические поля сразу
}

// ── Номер безопасности ────────────────────────────────────────────────────

void ContactProfileDialog::setSafetyNumber(const QString& safetyNum) {
    if (safetyNum.isEmpty()) {
        m_safetyLabel->setText(tr("—"));
        m_safetyHint->setText(tr("Сессия E2E ещё не установлена."));
    } else {
        m_safetyLabel->setText(safetyNum);
        m_safetyHint->setText(
            tr("Сверьте этот код с собеседником по голосовому/видео-звонку.\n"
               "Если коды совпадают — соединение защищено."));
    }
}

// ── Горячее обновление (пинг, аптайм, статус) ─────────────────────────────

void ContactProfileDialog::refreshData() {
    const PeerPublicInfo info = m_network->getPeerInfo(m_uuid);

    // Статус
    const bool online = info.state == ConnectionState::Connected;
    m_statusLabel->setText(online ? tr("● Онлайн") : tr("○ Офлайн"));

    // Кнопка шелла доступна только когда пир онлайн
    if (m_shellBtn) m_shellBtn->setEnabled(online);

    // Пинг
    if (info.latencyMs >= 0)
        m_pingRow->setText(QString("%1 мс").arg(info.latencyMs));
    else
        m_pingRow->setText(tr("—"));

    // Аптайм
    if (info.connectedSince.isValid() && online)
        m_uptimeRow->setText(formatUptime(info.connectedSince));
    else
        m_uptimeRow->setText(tr("—"));
}

// ── Форматирование времени соединения ─────────────────────────────────────

QString ContactProfileDialog::formatUptime(const QDateTime& since) const {
    const qint64 secs = since.secsTo(QDateTime::currentDateTime());
    if (secs < 0) return tr("—");

    const int h = static_cast<int>(secs / 3600);
    const int m = static_cast<int>((secs % 3600) / 60);

    if (h > 0)
        return QString(tr("%1ч %2м")).arg(h).arg(m);
    return QString(tr("%1м")).arg(m);
}
