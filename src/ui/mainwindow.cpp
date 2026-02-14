#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "chatwidget.h"
#include "contactswidget.h"
#include "settingspanel.h"
#include "updatebanner.h"
#include "thememanager.h"
#include "dialogs/addcontactdialog.h"
#include "dialogs/incomingdialog.h"
#include "../core/identity.h"
#include "../core/updatechecker.h"
#include "../core/demomode.h"
#include "../core/sessionmanager.h"
#include <QTimer>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QFileDialog>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Messenger");
    resize(1020, 700);
    setMinimumSize(720, 520);

    applyTheme();
    setupUi();

    auto& id = Identity::instance();
    id.load();

    m_storage = new StorageManager(this);
    m_storage->open();

    m_e2e = new E2EManager(this);
    m_e2e->init(id.uuid());

    m_network = new NetworkManager(this);
    m_fileTransfer = new FileTransfer(m_network, m_e2e, this);

    connect(m_network, &NetworkManager::ready,             this, &MainWindow::onAppReady);
    connect(m_network, &NetworkManager::incomingRequest,   this, &MainWindow::onIncomingRequest);
    connect(m_network, &NetworkManager::peerConnected,     this, &MainWindow::onPeerConnected);
    connect(m_network, &NetworkManager::peerDisconnected,  this, &MainWindow::onPeerDisconnected);
    connect(m_network, &NetworkManager::messageReceived,   this, &MainWindow::onMessageReceived);
    connect(m_e2e,     &E2EManager::sessionEstablished,    this, &MainWindow::onSessionEstablished);
    connect(m_contacts, &ContactsWidget::contactSelected,  this, &MainWindow::onContactSelected);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](Theme) { applyTheme(); });

    // При переключении demo-mode обновляем всё что показывает наши данные
    connect(&DemoMode::instance(), &DemoMode::toggled,
            this, [this](bool) { refreshOwnDisplay(); });

    m_network->init();
    m_contacts->setContacts(m_storage->allContacts());

    // Тихая проверка обновлений — не дёргает если проверяли < 6 часов назад
    auto* checker = new UpdateChecker(this);
    connect(checker, &UpdateChecker::updateAvailable,
            this, [this](const UpdateInfo& info) {
                m_updateBanner->showUpdate(info);
            });
    checker->checkInBackground();
}

MainWindow::~MainWindow() { delete ui; }

// ── UI Setup ──────────────────────────────────────────────────────────────

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->setSpacing(0);
    setCentralWidget(central);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setObjectName("mainSplitter");
    splitter->setHandleWidth(4);  // чуть шире чтобы легче захватить
    splitter->setChildrenCollapsible(false);  // не даём схлопнуть панель в 0

    // ══════════════════════════════════════════════════════════════════════
    // Левая колонка — QStackedWidget: [0]=чаты  [1]=настройки
    // ══════════════════════════════════════════════════════════════════════
    m_leftStack = new QStackedWidget();
    // Минимум 200, максимум 480 — как в TG
    m_leftStack->setMinimumWidth(200);
    m_leftStack->setMaximumWidth(480);

    // ── Страница 0: чаты ─────────────────────────────────────────────────
    auto* chatsPage = new QWidget();
    chatsPage->setObjectName("leftPanel");
    auto* chatsLayout = new QVBoxLayout(chatsPage);
    chatsLayout->setContentsMargins(0,0,0,0);
    chatsLayout->setSpacing(0);

    // Хедер
    auto* headerBar = new QWidget();
    headerBar->setObjectName("headerBar");
    headerBar->setFixedHeight(62);
    auto* headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(14, 0, 10, 0);
    headerLayout->setSpacing(8);

    auto* myAvatar = new QLabel();
    myAvatar->setObjectName("peerAvatar");
    myAvatar->setFixedSize(36, 36);
    myAvatar->setAlignment(Qt::AlignCenter);
    myAvatar->setText(Identity::instance().displayName().left(1).toUpper());

    m_nameLabel = new QLabel(Identity::instance().displayName());
    m_nameLabel->setObjectName("myNameLabel");
    m_nameLabel->setCursor(Qt::PointingHandCursor);

    auto* idBtn = new QPushButton("⊕");
    idBtn->setObjectName("iconBtn");
    idBtn->setFixedSize(32, 32);
    idBtn->setToolTip("Мой ID");
    connect(idBtn, &QPushButton::clicked, this, &MainWindow::onShowMyId);

    auto* editBtn = new QPushButton("✎");
    editBtn->setObjectName("iconBtn");
    editBtn->setFixedSize(32, 32);
    editBtn->setToolTip("Изменить имя");
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::onEditName);

    headerLayout->addWidget(myAvatar);
    headerLayout->addWidget(m_nameLabel, 1);
    headerLayout->addWidget(idBtn);
    headerLayout->addWidget(editBtn);

    // Контакты
    m_contacts = new ContactsWidget(chatsPage);

    // Баннер обновления — появляется между хедером и контактами
    m_updateBanner = new UpdateBanner(chatsPage);

    // Кнопка добавить
    auto* addBtn = new QPushButton("+ Добавить контакт");
    addBtn->setObjectName("addContactBtn");
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::onAddContactClicked);

    // Футер — шестерёнка + три кнопки тем
    auto* footer = new QWidget();
    footer->setObjectName("themeFooter");
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(10, 8, 10, 10);
    footerLayout->setSpacing(6);

    auto* settingsBtn = new QPushButton("⚙");
    settingsBtn->setObjectName("iconBtn");
    settingsBtn->setFixedSize(32, 32);
    settingsBtn->setToolTip("Настройки");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);

    const struct { Theme theme; QString label; } themes[] = {
        { Theme::Dark,  "◐  Тёмная"  },
        { Theme::Light, "○  Светлая" },
        { Theme::BW,    "●  Ч/Б"     },
    };
    const Theme current = ThemeManager::instance().currentTheme();
    for (const auto& t : themes) {
        auto* btn = new QPushButton(t.label, footer);
        btn->setObjectName("themePillBtn");
        btn->setCheckable(true);
        btn->setChecked(t.theme == current);
        connect(btn, &QPushButton::clicked, this, [btn, footer, t]() {
            for (auto* b : footer->findChildren<QPushButton*>("themePillBtn"))
                b->setChecked(false);
            btn->setChecked(true);
            ThemeManager::instance().setTheme(t.theme);
        });
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
                btn, [btn, t](Theme nt) { btn->setChecked(nt == t.theme); });
        footerLayout->addWidget(btn, 1);
    }
    footerLayout->insertWidget(0, settingsBtn);

    chatsLayout->addWidget(headerBar);
    chatsLayout->addWidget(m_updateBanner);   // скрыт по умолчанию
    chatsLayout->addWidget(m_contacts, 1);
    chatsLayout->addWidget(addBtn);
    chatsLayout->addWidget(footer);

    // ── Страница 1: настройки ─────────────────────────────────────────────
    m_settings = new SettingsPanel();
    connect(m_settings, &SettingsPanel::backRequested,
            this, &MainWindow::closeSettings);
    connect(m_settings, &SettingsPanel::nameChanged,
            this, [this](const QString& name) {
                m_nameLabel->setText(name);
            });

    m_leftStack->addWidget(chatsPage);   // index 0
    m_leftStack->addWidget(m_settings);  // index 1
    m_leftStack->setCurrentIndex(0);

    // ── Правая панель — чат ───────────────────────────────────────────────
    m_chat = new ChatWidget();
    connect(m_chat, &ChatWidget::sendMessage,       this, &MainWindow::onSendMessage);
    connect(m_chat, &ChatWidget::sendFileRequested, this, &MainWindow::onSendFile);

    splitter->addWidget(m_leftStack);
    splitter->addWidget(m_chat);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    // Восстанавливаем ширину левой панели из session.json
    const int savedWidth = SessionManager::instance().leftPanelWidth();
    splitter->setSizes({savedWidth, width() - savedWidth});

    // Сохраняем при каждом изменении — с дебаунсом чтобы не спамить на диск
    auto* saveTimer = new QTimer(this);
    saveTimer->setSingleShot(true);
    saveTimer->setInterval(500);  // 500мс после последнего движения
    connect(splitter, &QSplitter::splitterMoved,
            this, [splitter, saveTimer](int, int) {
        const int w = splitter->sizes().value(0, 280);
        SessionManager::instance().setLeftPanelWidth(w);
        // Перезапускаем таймер — пишем на диск только когда перестали двигать
        saveTimer->start();
    });

    mainLayout->addWidget(splitter);

    statusBar()->showMessage("Инициализация...");
}

void MainWindow::openSettings() {
    m_settings->reload();
    m_leftStack->setCurrentIndex(1);
    m_chat->showPlaceholder();
}

void MainWindow::closeSettings() {
    m_leftStack->setCurrentIndex(0);
    // Восстанавливаем чат если был активный пир
    if (!m_activePeer.isNull()) {
        const Contact c = m_storage->getContact(m_activePeer);
        m_chat->openConversation(c.name, m_network->isOnline(m_activePeer));
        m_chat->loadHistory(m_storage->getMessages(m_activePeer, 100));
    }
}

void MainWindow::applyTheme() {
    auto& tm = ThemeManager::instance();
    const auto& p = tm.palette();

    // Сначала сбрасываем stylesheet чтобы Qt не кэшировал старый
    qApp->setStyleSheet("");

    qApp->setStyle("Fusion");

    QPalette pal;
    pal.setColor(QPalette::Window,          QColor(p.bg));
    pal.setColor(QPalette::WindowText,      QColor(p.textPrimary));
    pal.setColor(QPalette::Base,            QColor(p.bgInput));
    pal.setColor(QPalette::AlternateBase,   QColor(p.bgElevated));
    pal.setColor(QPalette::Text,            QColor(p.textPrimary));
    pal.setColor(QPalette::Button,          QColor(p.bgElevated));
    pal.setColor(QPalette::ButtonText,      QColor(p.textPrimary));
    pal.setColor(QPalette::Highlight,       QColor(p.accent));
    pal.setColor(QPalette::HighlightedText, QColor(p.textOnAccent));
    pal.setColor(QPalette::PlaceholderText, QColor(p.textMuted));
    qApp->setPalette(pal);

    // Применяем новый stylesheet
    qApp->setStyleSheet(tm.stylesheet());

    // Принудительно repolish все top-level виджеты и их дочерние
    // Без этого Qt оставляет старые стили на уже созданных виджетах
    const auto widgets = qApp->allWidgets();
    for (auto* w : widgets) {
        if (!w) continue;
        w->setPalette(pal);
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }
}

void MainWindow::updateStatusBar(const QString& ip, quint16 port, bool upnp) {
    auto& dm = DemoMode::instance();
    const QString showIp   = dm.ip(ip.isEmpty() ? "Нет IP" : ip);
    const QString showPort = (dm.enabled() && port != 0)
        ? "00000"
        : QString::number(port);
    statusBar()->showMessage(
        QString("  %1:%2%3").arg(showIp, showPort)
            .arg(upnp ? "   UPnP ✓" : "   UPnP ✗"));
}

void MainWindow::refreshOwnDisplay() {
    auto& dm = DemoMode::instance();
    auto& id = Identity::instance();
    if (m_nameLabel)
        m_nameLabel->setText(dm.displayName(id.displayName()));
    updateStatusBar(m_lastIp, m_lastPort, m_lastUpnp);
}

// ── Slots ─────────────────────────────────────────────────────────────────

void MainWindow::onCycleTheme() {}

void MainWindow::onAppReady(const QString& ip, quint16 port, bool upnp) {
    m_lastIp   = ip;
    m_lastPort = port;
    m_lastUpnp = upnp;
    refreshOwnDisplay();
}

void MainWindow::onIncomingRequest(QUuid uuid, QString name, QString ip) {
    auto* dlg = new IncomingDialog(name, ip, this);
    if (dlg->exec() == QDialog::Accepted) {
        m_network->acceptIncoming(uuid);
        if (m_storage->getContact(uuid).uuid.isNull()) {
            Contact c;
            c.uuid = uuid; c.name = name; c.ip = ip;
            if (!m_storage->addContact(c))
                qWarning("[Main] Failed to save contact %s", qPrintable(name));
            m_contacts->setContacts(m_storage->allContacts());
        }
    } else {
        m_network->rejectIncoming(uuid);
    }
    dlg->deleteLater();
}

void MainWindow::onPeerConnected(QUuid uuid, QString name) {
    m_contacts->setPeerOnline(uuid, true);
    statusBar()->showMessage("  " + name + " подключился", 3000);

    QJsonObject keyMsg;
    keyMsg["type"]   = "KEY_BUNDLE";
    keyMsg["bundle"] = m_e2e->ourBundleJson();
    m_network->sendJson(uuid, keyMsg);
}

void MainWindow::onPeerDisconnected(QUuid uuid) {
    m_contacts->setPeerOnline(uuid, false);
    if (m_activePeer == uuid)
        m_chat->setPeerStatus("○ оффлайн");
}

void MainWindow::onMessageReceived(QUuid from, QJsonObject msg) {
    const QString type = msg["type"].toString();

    if (type == "KEY_BUNDLE") {
        if (!m_e2e->hasSession(from)) {
            const QJsonObject initMsg = m_e2e->initiateSession(from, msg["bundle"].toObject());
            if (!initMsg.isEmpty()) m_network->sendJson(from, initMsg);
        }
        return;
    }
    if (type == "KEY_INIT") {
        const QJsonObject reply = m_e2e->acceptSession(from, msg);
        if (!reply.isEmpty()) m_network->sendJson(from, reply);
        return;
    }
    if (type == "KEY_ACK") return;

    if (type == "CHAT") {
        const QByteArray plain = m_e2e->decrypt(from, msg);
        if (plain.isEmpty()) return;
        const QString text = QString::fromUtf8(plain);

        Message m;
        m.peerUuid = from; m.outgoing = false;
        m.text = text; m.timestamp = QDateTime::currentDateTime();
        m.ciphertext = QJsonDocument(msg).toJson();
        if (m_storage->saveMessage(m) <= 0)
            qWarning("[Main] Failed to save incoming message");

        if (m_activePeer == from)
            m_chat->appendMessage(text, false, QDateTime::currentDateTime());
        m_contacts->updateLastMessage(from, text);
        return;
    }
    m_fileTransfer->handleMessage(from, msg);
}

void MainWindow::onSessionEstablished(QUuid peerUuid) {
    qDebug("[Main] E2E session established with %s",
           qPrintable(peerUuid.toString()));
}

void MainWindow::onAddContactClicked() {
    auto* dlg = new AddContactDialog(this);
    if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }

    const QString connStr = dlg->connectionString();
    dlg->deleteLater();

    const auto peer = Identity::parseConnectionString(connStr);
    if (!peer) {
        QMessageBox::warning(this, "Неверный формат",
            "Строка подключения неверная.\nФормат: Имя@UUID@IP:Порт");
        return;
    }

    if (m_storage->getContact(peer->uuid).uuid.isNull()) {
        Contact c;
        c.uuid = peer->uuid; c.name = peer->name;
        c.ip = peer->ip;     c.port = peer->port;
        if (!m_storage->addContact(c))
            qWarning("[Main] Failed to save new contact");
        m_contacts->setContacts(m_storage->allContacts());
    }

    m_network->connectToPeer(*peer);
    statusBar()->showMessage("  Подключаемся к " + peer->name + "...", 4000);
}

void MainWindow::onContactSelected(QUuid uuid) {
    m_activePeer = uuid;
    const Contact c = m_storage->getContact(uuid);
    m_chat->openConversation(c.name, m_network->isOnline(uuid));
    m_chat->loadHistory(m_storage->getMessages(uuid, 100));
}

void MainWindow::onSendMessage(const QString& text) {
    if (m_activePeer.isNull() || text.trimmed().isEmpty()) return;
    if (!m_e2e->hasSession(m_activePeer)) {
        QMessageBox::warning(this, "E2E не готов",
            "Сессия шифрования ещё не установлена. Подожди секунду.");
        return;
    }

    const QJsonObject env = m_e2e->encrypt(m_activePeer, text.toUtf8());
    if (env.isEmpty()) return;
    m_network->sendJson(m_activePeer, env);

    Message msg;
    msg.peerUuid = m_activePeer; msg.outgoing = true;
    msg.text = text; msg.timestamp = QDateTime::currentDateTime();
    msg.ciphertext = QJsonDocument(env).toJson();
    if (m_storage->saveMessage(msg) <= 0)
        qWarning("[Main] Failed to save outgoing message");

    m_chat->appendMessage(text, true, QDateTime::currentDateTime());
    m_contacts->updateLastMessage(m_activePeer, text);
}

void MainWindow::onSendFile() {
    if (m_activePeer.isNull()) return;
    const QString path = QFileDialog::getOpenFileName(this, "Выбери файл");
    if (path.isEmpty()) return;
    m_fileTransfer->sendFile(m_activePeer, path);
}

void MainWindow::onShowMyId() {
    const auto& id = Identity::instance();
    const QString connStr = id.connectionString(
        m_network->externalIp(), m_network->localPort());

    QMessageBox dlg(this);
    dlg.setWindowTitle("Мой ID");
    dlg.setText("<b>Отправь эту строку своему контакту:</b>");
    dlg.setInformativeText(connStr);
    auto* copyBtn = dlg.addButton("  Скопировать  ", QMessageBox::ActionRole);
    dlg.addButton("Закрыть", QMessageBox::RejectRole);
    dlg.exec();
    if (dlg.clickedButton() == copyBtn) {
        QApplication::clipboard()->setText(connStr);
        statusBar()->showMessage("  Скопировано!", 2000);
    }
}

void MainWindow::onEditName() {
    bool ok;
    const QString name = QInputDialog::getText(
        this, "Изменить имя", "Новое имя:",
        QLineEdit::Normal, Identity::instance().displayName(), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        Identity::instance().setDisplayName(name.trimmed());
        m_nameLabel->setText(name.trimmed());
    }
}
