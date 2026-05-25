#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "../core/app.h"
#include "../core/network.h"
#include "../core/storage.h"
#include "../core/filetransfer.h"
#include "../core/callmanager.h"
#include "../core/remoteshellmanager.h"
#include "../crypto/e2e.h"
#include "callwindow.h"
#ifdef Q_OS_WIN
#include "../platform/windowsfirewall.h"
#endif
#include "chatwidget.h"
#include "contactswidget.h"
#include "settingspanel.h"
#include "updatebanner.h"
#include "thememanager.h"
#include "dialogs/addcontactdialog.h"
#include "dialogs/incomingdialog.h"
#include "dialogs/contactprofiledialog.h"
#include "dialogs/fileacceptdialog.h"
#include "shellwindow.h"
#include "shellmonitor.h"
#include "sidedrawer.h"
#include "../media/mediaengine.h"
#include "../core/identity.h"
#include "../core/updatechecker.h"
#include "../core/versionutils.h"
#include "../core/demomode.h"
#include "../core/sessionmanager.h"
#include "../core/logger.h"
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
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QHostAddress>
#include <QMenu>
#include <QLineEdit>
#include <QCloseEvent>
#include <QIcon>
#include <QSize>

MainWindow::MainWindow(App& app, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_storage(&app.storage())
    , m_e2e(&app.e2e())
    , m_network(&app.network())
    , m_fileTransfer(&app.fileTransfer())
    , m_callManager(&app.callManager())
    , m_shellManager(&app.shellManager())
{
    ui->setupUi(this);
    setWindowTitle("Naleystogramm");
    resize(1020, 700);
    setMinimumSize(720, 520);

    applyTheme();
    setupUi();

    // ── Голосовые звонки ─────────────────────────────────────────────────────
    connect(m_chat, &ChatWidget::callRequested,
            this, &MainWindow::onCallRequested);
    connect(m_callManager, &CallManager::incomingCall,
            this, &MainWindow::onIncomingCall);
    connect(m_callManager, &CallManager::callAccepted,
            this, [this](QUuid /*peer*/) {
        if (m_callWindow) m_callWindow->setState(CallWindow::State::InCall);
    });
    connect(m_callManager, &CallManager::callRejected,
            this, [this](QUuid /*peer*/, const QString& reason) {
        if (m_callWindow) {
            m_callWindow->hide();
            m_callWindow->deleteLater();
            m_callWindow = nullptr;
        }
        const QString msg = (reason == "busy") ? "Пир занят" :
                            (reason == "timeout") ? "Нет ответа" :
                            "Звонок отклонён";
        QMessageBox::information(this, "Звонок", msg);
    });
    connect(m_callManager, &CallManager::callEnded,
            this, &MainWindow::onCallEnded);
    connect(m_callManager, &CallManager::callError,
            this, [this](const QString& msg) {
        QMessageBox::warning(this, "Ошибка звонка", msg);
    });

    // Голосовые: отправка из ChatWidget
    connect(m_chat, &ChatWidget::sendVoiceRequested,
            this, &MainWindow::onSendVoice);

    // ── Удалённый шелл ───────────────────────────────────────────────────────
    connect(m_shellManager, &RemoteShellManager::shellChallengeGenerated,
            this, &MainWindow::onShellChallengeGenerated);
    connect(m_shellManager, &RemoteShellManager::shellSessionStarted,
            this, &MainWindow::onShellSessionStarted);
    connect(m_shellManager, &RemoteShellManager::shellPasswordRequired,
            this, &MainWindow::onShellPasswordRequired);
    connect(m_shellManager, &RemoteShellManager::shellAccepted,
            this, &MainWindow::onShellAccepted);
    connect(m_shellManager, &RemoteShellManager::shellRejected,
            this, &MainWindow::onShellRejected);
    connect(m_shellManager, &RemoteShellManager::dataReceived,
            this, &MainWindow::onShellDataReceived);
    connect(m_shellManager, &RemoteShellManager::inputMonitored,
            this, &MainWindow::onInputMonitored);
    connect(m_shellManager, &RemoteShellManager::sessionEnded,
            this, &MainWindow::onShellSessionEnded);
    connect(m_shellManager, &RemoteShellManager::privilegeEscalationDetected,
            this, &MainWindow::onPrivilegeEscalationDetected);

    // Входящий файл или голосовое — диалог или авто-принятие
    connect(m_fileTransfer, &FileTransfer::fileOffer,
            this, [this](QUuid from, QString name, qint64 size,
                         QString offerId, int durationMs) {
        // Проверяем конфиденциальность: голосовые и файлы — разные уровни
        const auto& sm = SessionManager::instance();
        if (durationMs > 0) {
            if (!checkPrivacy(sm.privacyVoice(), from)) {
                qDebug("[Main] Голосовое от %s отклонено (настройки конфиденциальности)",
                       qPrintable(from.toString(QUuid::WithoutBraces)));
                m_fileTransfer->rejectOffer(from, offerId);
                return;
            }
        } else {
            if (!checkPrivacy(sm.privacyFiles(), from)) {
                qDebug("[Main] Файл от %s отклонен (настройки конфиденциальности)",
                       qPrintable(from.toString(QUuid::WithoutBraces)));
                m_fileTransfer->rejectOffer(from, offerId);
                return;
            }
        }

        m_pendingTransferSenders[offerId] = from;
        if (durationMs > 0) {
            // Голосовое сообщение — принимаем без диалога
            m_pendingVoiceDurations[offerId] = durationMs;
            m_fileTransfer->acceptOffer(from, offerId);
        } else {
            // Обычный файл — показываем диалог подтверждения
            const Contact c = m_storage->getContact(from);
            const QString senderName = c.uuid.isNull()
                ? from.toString(QUuid::WithoutBraces).left(8)
                : c.name;
            auto* dlg = new FileAcceptDialog(senderName, name, size, offerId, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            connect(dlg, &FileAcceptDialog::accepted,
                    this, [this, from](const QString& id) {
                m_fileTransfer->acceptOffer(from, id);
            });
            connect(dlg, &FileAcceptDialog::rejected,
                    this, [this, from](const QString& id) {
                m_fileTransfer->rejectOffer(from, id);
                m_pendingTransferSenders.remove(id);
            });
            dlg->show();
        }
    });

    // Передача завершена — сохранить в БД и показать в чате
    connect(m_fileTransfer, &FileTransfer::transferCompleted,
            this, [this](QString id, QString filePath, bool outgoing) {
        const int  durationMs = m_pendingVoiceDurations.take(id);
        const QUuid peer = outgoing
            ? m_activePeer
            : m_pendingTransferSenders.take(id);

        if (peer.isNull()) return;

        const QDateTime now = QDateTime::currentDateTime();
        if (durationMs > 0) {
            // Голосовое сообщение
            Message voiceMsg;
            voiceMsg.peerUuid       = peer;
            voiceMsg.outgoing       = outgoing;
            voiceMsg.isVoice        = true;
            voiceMsg.voiceDurationMs = durationMs;
            voiceMsg.text           = filePath;  // путь к файлу для воспроизведения
            voiceMsg.timestamp      = now;
            if (m_storage->saveMessage(voiceMsg) <= 0)
                qWarning("[Main] Не удалось сохранить голосовое сообщение");

            if (!outgoing && m_activePeer == peer)
                m_chat->appendVoiceMessage(false, durationMs, now, filePath);
        } else {
            // Обычный файл
            const QString fileName = QFileInfo(filePath).fileName();
            if (!outgoing) {
                Message fileMsg;
                fileMsg.peerUuid  = peer;
                fileMsg.outgoing  = false;
                fileMsg.fileName  = fileName;
                fileMsg.timestamp = now;
                if (m_storage->saveMessage(fileMsg) <= 0)
                    qWarning("[Main] Не удалось сохранить входящий файл");

                if (m_activePeer == peer)
                    m_chat->appendMessage(
                        QString("⊕ %1").arg(fileName), false, now);
            }
            statusBar()->showMessage(
                outgoing
                    ? tr("Файл отправлен: %1").arg(fileName)
                    : tr("Файл получен: %1").arg(fileName),
                4000);
        }
    });

    // Подключаем логирование сети к централизованному логгеру
    connect(m_network, &NetworkManager::connectionLog, this, [](const QString& msg) {
        Logger::instance().info(LogComponent::Network, msg);
    });
    connect(m_settings, &SettingsPanel::verboseLoggingChanged,
            m_network,  &NetworkManager::setVerboseLogging);

    connect(m_network, &NetworkManager::ready,             this, &MainWindow::onAppReady);
    connect(m_network, &NetworkManager::ready,
            m_settings, [this](const QString& ip, quint16 port, bool) {
                m_settings->setExternalAddress(ip, port);
            });
    connect(m_network, &NetworkManager::externalIpDiscovered,
            m_settings, [this](const QString& ip) {
                m_settings->setExternalAddress(ip, m_network->advertisedPort());
            });
    connect(m_network, &NetworkManager::incomingRequest,   this, &MainWindow::onIncomingRequest);
    connect(m_network, &NetworkManager::peerConnected,     this, &MainWindow::onPeerConnected);
    connect(m_network, &NetworkManager::peerDisconnected,  this, &MainWindow::onPeerDisconnected);
    connect(m_network, &NetworkManager::messageReceived,   this, &MainWindow::onMessageReceived);
    connect(m_network, &NetworkManager::contactNameUpdated,this, &MainWindow::onContactNameUpdated);

    // Обновляем индикатор присутствия при каждом изменении состояния подключения.
    // Connected → 🟢, Reconnecting/Connecting → 🟡, Disconnected → ⚫
    connect(m_network, &NetworkManager::connectionStateChanged,
            this, [this](QUuid uuid, ConnectionState state) {
                PeerPresence presence;
                switch (state) {
                case ConnectionState::Connected:
                    presence = PeerPresence::Online;
                    break;
                case ConnectionState::Reconnecting:
                case ConnectionState::Connecting:
                    presence = PeerPresence::Reconnecting;
                    break;
                default:
                    presence = PeerPresence::Offline;
                    break;
                }
                m_contacts->setPeerPresence(uuid, presence);
            });

    // Внешний IP может прийти раньше или позже UPnP — обновляем статус-бар сразу
    connect(m_network, &NetworkManager::externalIpDiscovered, this, [this](const QString& ip) {
        m_lastIp = ip;
        updateStatusBar(m_lastIp, m_lastPort, m_lastUpnp);
    });

    // UPnP завершается асинхронно — обновляем кнопку и статус-бар
    connect(m_network, &NetworkManager::upnpMappingResult, this, [this](bool ok) {
        m_lastUpnp = ok;
        updateStatusBar(m_lastIp, m_lastPort, m_lastUpnp);
        if (m_upnpBtn) {
            m_upnpBtn->setText(ok ? tr("UPnP ✓") : tr("UPnP ✗"));
            // При неудаче — кнопка активна для повтора, при успехе — просто статус
            m_upnpBtn->setEnabled(!ok);
            m_upnpBtn->setCursor(ok ? Qt::ArrowCursor : Qt::PointingHandCursor);
            m_upnpBtn->setToolTip(ok
                ? tr("UPnP: порты успешно проброшены (%1)").arg(m_lastPort)
                : tr("Не удалось открыть порты автоматически.\n"
                     "Пробросьте порты 47821–47841 вручную в настройках роутера\n"
                     "или включите DMZ.\n\nНажмите для повторной попытки."));
        }
    });

    // Open Port: запускаем проверку доступности после обнаружения внешнего IP
    connect(m_network, &NetworkManager::openPortCheckResult, this, [this](bool open) {
        if (!m_openPortBtn) return;
        const quint16 port = SessionManager::instance().manualPublicPort();
        m_openPortBtn->setText(
            open ? tr("Open Port: %1 ✓").arg(port)
                 : tr("Open Port: %1 ✗").arg(port));
        m_openPortBtn->setToolTip(
            open ? tr("Порт %1 доступен снаружи").arg(port)
                 : tr("Порт %1 недоступен или роутер не поддерживает hairpin NAT.\n"
                      "Попробуйте подключиться с другого устройства чтобы убедиться\n"
                      "что порт реально открыт в роутере.").arg(port));
    });
    connect(m_e2e,     &E2EManager::sessionEstablished,    this, &MainWindow::onSessionEstablished);
    connect(m_contacts, &ContactsWidget::contactSelected,  this, &MainWindow::onContactSelected);

    // Профиль контакта — кнопки в двух местах UI
    connect(m_chat,     &ChatWidget::openProfileRequested,
            this, &MainWindow::onOpenProfile);
    connect(m_contacts, &ContactsWidget::profileRequested,
            this, &MainWindow::onOpenProfile);

    // Действия контекстного меню контактов
    connect(m_contacts, &ContactsWidget::blockRequested,
            this, &MainWindow::onBlockContact);
    connect(m_contacts, &ContactsWidget::muteRequested,
            this, [this](const QUuid& uuid) {
                Contact c = m_storage->getContact(uuid);
                if (c.uuid.isNull()) return;
                (void)m_storage->setContactMuted(uuid, !c.isMuted);
                m_contacts->setContacts(m_storage->allContacts());
            });
    connect(m_contacts, &ContactsWidget::deleteChatRequested,
            this, &MainWindow::onDeleteChat);
    connect(m_contacts, &ContactsWidget::contactDeleteRequested,
            this, &MainWindow::onDeleteContact);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](Theme) { applyTheme(); });

    // При переключении demo-mode обновляем всё что показывает наши данные
    connect(&DemoMode::instance(), &DemoMode::toggled,
            this, [this](bool) { refreshOwnDisplay(); });

    m_network->init();
    m_contacts->setContacts(m_storage->allContacts());

#ifdef Q_OS_WIN
    // Ждём 1 секунду чтобы главное окно успело отобразиться перед диалогом
    QTimer::singleShot(1000, this, [this]() {
        WindowsFirewall::checkAndPrompt(this, m_network->localPort());
    });
#endif

    // Тихая проверка обновлений — не дёргает если проверяли < 6 часов назад
    auto* checker = new UpdateChecker(this);
    connect(checker, &UpdateChecker::updateAvailable,
            this, [this](const UpdateInfo& info) {
                m_updateBanner->showUpdate(info);
            });
    checker->checkInBackground();

    // ── System tray ──────────────────────────────────────────────────────────
    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icons/app_icon.png")), this);
    auto* trayMenu = new QMenu(this);
    auto* showAct = trayMenu->addAction(tr("Открыть"));
    trayMenu->addSeparator();
    auto* quitAct = trayMenu->addAction(tr("Выйти"));
    m_tray->setContextMenu(trayMenu);
    m_tray->setToolTip("Naleystogramm");
    m_tray->show();

    connect(showAct, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger) {
            if (isHidden() || isMinimized()) { showNormal(); raise(); activateWindow(); }
            else hide();
        }
    });

    // ── Typing indicators (исходящие) ────────────────────────────────────────
    connect(m_chat, &ChatWidget::typingStarted, this, [this]() {
        if (m_activePeer.isNull()) return;
        m_network->sendJson(m_activePeer,
            QJsonObject{{"type", "TYPING"}, {"state", "start"}});
    });
    connect(m_chat, &ChatWidget::typingStopped, this, [this]() {
        if (m_activePeer.isNull()) return;
        m_network->sendJson(m_activePeer,
            QJsonObject{{"type", "TYPING"}, {"state", "stop"}});
    });
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
    } else {
        QMainWindow::closeEvent(event);
    }
}

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

    // ── Компактный топбар: гамбургер-меню + поиск ───────────────────────────
    auto* topBar = new QWidget();
    topBar->setObjectName("headerBar");
    topBar->setFixedHeight(54);
    auto* topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(8, 0, 8, 0);
    topBarLayout->setSpacing(8);

    m_hamburgerBtn = new QPushButton();
    m_hamburgerBtn->setObjectName("iconBtn");
    m_hamburgerBtn->setFixedSize(36, 36);
    m_hamburgerBtn->setToolTip(tr("Меню"));
    ThemeManager::applyIcon(m_hamburgerBtn, QStringLiteral(":/icons/dialogs_menu.png"), QSize(20, 20));

    m_searchEdit = new QLineEdit();
    m_searchEdit->setObjectName("searchInput");
    m_searchEdit->setPlaceholderText(tr("Поиск..."));
    m_searchEdit->setFixedHeight(36);

    topBarLayout->addWidget(m_hamburgerBtn);
    topBarLayout->addWidget(m_searchEdit, 1);

    // Гамбургер → боковой ящик
    connect(m_hamburgerBtn, &QPushButton::clicked, this, [this]() {
        const QString name = Identity::instance().displayName();
        const QString ownPath = SessionManager::instance().avatarPath();
        QPixmap avatar((!ownPath.isEmpty() && QFile::exists(ownPath))
                       ? ownPath : ":/icons/not-avatar.png");
        m_sideDrawer->open(name, avatar);
    });

    // Контакты
    m_contacts = new ContactsWidget(chatsPage);

    // Боковой ящик (поверх контактов, дочерний для chatsPage)
    m_sideDrawer = new SideDrawer(chatsPage);
    connect(m_sideDrawer, &SideDrawer::editNameRequested,   this, &MainWindow::onEditName);
    connect(m_sideDrawer, &SideDrawer::showIdRequested,     this, &MainWindow::onShowMyId);
    connect(m_sideDrawer, &SideDrawer::addContactRequested, this, &MainWindow::onAddContactClicked);
    connect(m_sideDrawer, &SideDrawer::settingsRequested,   this, &MainWindow::openSettings);

    m_updateBanner = new UpdateBanner(chatsPage);

    // Поиск соединяем с фильтром контактов
    connect(m_searchEdit, &QLineEdit::textChanged, m_contacts, &ContactsWidget::setFilter);

    chatsLayout->addWidget(topBar);
    chatsLayout->addWidget(m_contacts, 1);
    chatsLayout->addWidget(m_updateBanner);   // внизу, скрыт по умолчанию

    m_leftStack->addWidget(chatsPage);
    m_leftStack->setCurrentIndex(0);

    // ── Настройки — overlay поверх всего central widget ──────────────────
    m_settings = new SettingsPanel(central);
    connect(m_settings, &SettingsPanel::nameChanged,
            this, [this](const QString& name) {
                if (m_network) m_network->broadcastProfileUpdate(name);
            });
    connect(m_settings, &SettingsPanel::avatarChanged,
            this, [this](const QString&) { loadOwnAvatar(); });
    connect(m_settings, &SettingsPanel::enterSendsChanged,
            this, [this](bool on) { m_chat->setEnterSends(on); });
    connect(m_settings, &SettingsPanel::connectToDeviceRequested,
            this, [this](const QString& host, quint16 port, const QString& code) {
                if (m_network) m_network->connectToDevice(host, port, code);
            });

    // ── Правая панель — чат ───────────────────────────────────────────────
    m_chat = new ChatWidget();
    // Применяем сохранённую настройку режима Enter при создании виджета
    m_chat->setEnterSends(SessionManager::instance().enterSends());
    connect(m_chat, &ChatWidget::sendMessage,       this, &MainWindow::onSendMessage);
    connect(m_chat, &ChatWidget::sendFileRequested, this, &MainWindow::onSendFile);

    // Lazy loading истории: прокрутка вверх → подгружаем следующую порцию из DB
    connect(m_chat, &ChatWidget::loadMoreRequested, this, [this]() {
        if (m_activePeer.isNull()) return;
        const int newOffset = m_chat->historyOffset() + 50;
        m_chat->setHistoryOffset(newOffset);
        const auto older = m_storage->getMessages(m_activePeer, 50, newOffset);
        m_chat->prependHistory(older);  // сбрасывает m_loadingMore даже при пустом результате
    });

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

    // Индикатор подключения в статус-баре — UPnP или Open Port в зависимости от режима
    const bool isOpenPortMode =
        (SessionManager::instance().portForwardingMode() == PortForwardingMode::OpenPort);

    if (isOpenPortMode) {
        // Режим «Разблокированный порт»: показываем порт и результат проверки
        const quint16 openPort = SessionManager::instance().manualPublicPort();
        m_openPortBtn = new QPushButton(
            tr("Open Port: %1 ...").arg(openPort));
        m_openPortBtn->setObjectName("upnpStatusBtn");
        m_openPortBtn->setFlat(true);
        m_openPortBtn->setFixedHeight(20);
        m_openPortBtn->setCursor(Qt::ArrowCursor);
        m_openPortBtn->setEnabled(false);
        m_openPortBtn->setToolTip(tr("Проверяем доступность порта %1...").arg(openPort));
        statusBar()->addPermanentWidget(m_openPortBtn);
    } else {
        // Режим UPnP (по умолчанию)
        m_upnpBtn = new QPushButton(tr("UPnP ..."));
        m_upnpBtn->setObjectName("upnpStatusBtn");
        m_upnpBtn->setFlat(true);
        m_upnpBtn->setFixedHeight(20);
        m_upnpBtn->setCursor(Qt::ArrowCursor);
        m_upnpBtn->setEnabled(false);
        m_upnpBtn->setToolTip(tr("Проверяем UPnP..."));
        connect(m_upnpBtn, &QPushButton::clicked, this, [this]() {
            if (!m_network) return;
            statusBar()->showMessage(tr("Повторяем пробрасывание портов..."), 3000);
            m_upnpBtn->setEnabled(false);
            m_upnpBtn->setText(tr("UPnP ..."));
            m_upnpBtn->setToolTip(tr("Проверяем..."));
            m_network->retryUpnp();
        });
        statusBar()->addPermanentWidget(m_upnpBtn);
    }

    loadOwnAvatar();
    statusBar()->showMessage(tr("Инициализация..."));
}

void MainWindow::openSettings() {
    if (m_sideDrawer->isVisible()) {
        // Дроуер ещё на экране (открыт или закрывается) — ждём конца анимации,
        // чтобы он не попал в захват фона для блюра
        connect(m_sideDrawer, &SideDrawer::closed, this, [this]() {
            m_settings->openPanel();
        }, Qt::SingleShotConnection);
        if (m_sideDrawer->isOpen())
            m_sideDrawer->closeDrawer();
        // иначе анимация закрытия уже идёт — просто ждём сигнала closed()
    } else {
        m_settings->openPanel();
    }
}

void MainWindow::closeSettings() {
    m_settings->closePanel();
}

void MainWindow::loadOwnAvatar() {
    if (!m_myAvatar) return;

    const QString ownPath = SessionManager::instance().avatarPath();
    const QString path = (!ownPath.isEmpty() && QFile::exists(ownPath))
        ? ownPath
        : QStringLiteral(":/icons/not-avatar.png");

    QPixmap src(path);
    if (src.isNull()) return;

    const int sz = m_myAvatar->width();
    const QPixmap scaled = src.scaled(sz, sz,
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPixmap rounded(sz, sz);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clipPath;
    clipPath.addEllipse(0, 0, sz, sz);
    painter.setClipPath(clipPath);
    painter.drawPixmap(0, 0, scaled);

    m_myAvatar->setText({});
    m_myAvatar->setPixmap(rounded);
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
    const QString showIp   = dm.ip(ip.isEmpty() ? tr("Нет IP") : ip);
    const QString showPort = (dm.enabled() && port != 0)
        ? "00000"
        : (port == 0 ? "—" : QString::number(port));
    // IP:порт в текстовой части; UPnP статус — в постоянной кнопке справа
    statusBar()->showMessage(QString("  %1:%2").arg(showIp, showPort));

    // Синхронизируем кнопку (если ready пришёл раньше upnpMappingResult)
    if (m_upnpBtn && m_upnpBtn->text() == tr("UPnP ...") && port != 0) {
        // Ещё ждём результат — ничего не меняем
    } else if (m_upnpBtn && port != 0) {
        m_upnpBtn->setText(upnp ? tr("UPnP ✓") : tr("UPnP ✗"));
        m_upnpBtn->setEnabled(!upnp);
    }
}

void MainWindow::refreshOwnDisplay() {
    auto& dm = DemoMode::instance();
    auto& id = Identity::instance();
    if (m_nameLabel)
        m_nameLabel->setText(dm.displayName(id.displayName()));
    updateStatusBar(m_lastIp, m_lastPort, m_lastUpnp);
}

// ── Вспомогательные проверки конфиденциальности ──────────────────────────

bool MainWindow::isKnownContact(const QUuid& uuid) const {
    return !m_storage->getContact(uuid).uuid.isNull();
}

bool MainWindow::checkPrivacy(PrivacyLevel level, const QUuid& from) const {
    switch (level) {
        case PrivacyLevel::Everyone:     return true;
        case PrivacyLevel::ContactsOnly: return isKnownContact(from);
        case PrivacyLevel::Nobody:       return false;
    }
    return true;
}

// ── Slots ─────────────────────────────────────────────────────────────────

void MainWindow::onCycleTheme() {}

void MainWindow::onAppReady(const QString& ip, quint16 port, bool upnp) {
    m_lastIp   = ip;
    m_lastPort = port;
    m_lastUpnp = upnp;
    refreshOwnDisplay();

    // В режиме OpenPort запускаем проверку доступности порта
    if (m_openPortBtn && m_network &&
        SessionManager::instance().portForwardingMode() == PortForwardingMode::OpenPort)
    {
        m_network->checkOpenPort();
    }
}

void MainWindow::onIncomingRequest(QUuid uuid, QString name, QString ip) {
    // Известный контакт — принимаем переподключение автоматически без диалога.
    // Это критично для стабильной работы: без авто-принятия переподключение
    // требует ручного подтверждения каждый раз.
    if (!m_storage->getContact(uuid).uuid.isNull()) {
        m_network->acceptIncoming(uuid);
        statusBar()->showMessage(tr("%1 переподключился").arg(name), 4000);
        m_contacts->setPeerOnline(uuid, true);
        qDebug("[Main] Авто-принято переподключение от известного контакта: %s",
               qPrintable(name));
        // Синхронизируем имя при переподключении — пир мог сменить ник
        onContactNameUpdated(uuid, name);
        return;
    }

    // Новый неизвестный пир — показываем диалог подтверждения
    auto* dlg = new IncomingDialog(name, ip, this);
    if (dlg->exec() == QDialog::Accepted) {
        m_network->acceptIncoming(uuid);
        if (m_storage->getContact(uuid).uuid.isNull()) {
            Contact c;
            c.uuid = uuid; c.name = name; c.ip = ip;
            if (!m_storage->addContact(c))
                qWarning("[Main] Не удалось сохранить контакт %s", qPrintable(name));
            m_contacts->setContacts(m_storage->allContacts());
        }
    } else {
        m_network->rejectIncoming(uuid);
    }
    dlg->deleteLater();
}

void MainWindow::onPeerConnected(QUuid uuid, QString name) {
    m_contacts->setPeerOnline(uuid, true);
    statusBar()->showMessage(tr("%1 connected").arg(name), 3000);

    // Обновляем имя контакта из HANDSHAKE_ACK — пир сам сообщает своё актуальное имя
    onContactNameUpdated(uuid, name);

    QJsonObject keyMsg;
    keyMsg["type"]   = "KEY_BUNDLE";
    keyMsg["bundle"] = m_e2e->ourBundleJson();
    m_network->sendJson(uuid, keyMsg);

    // Сохраняем системную информацию пира в БД — чтобы показывалась и когда пир офлайн
    const auto info    = m_network->getPeerInfo(uuid);
    if (!info.systemInfo.isEmpty()) {
        if (!m_storage->updateContactSystemInfo(uuid, info.systemInfo))
            qWarning("[Main] Не удалось сохранить systemInfo для %s",
                     qPrintable(name));
    }
    if (!info.birthday.isEmpty())
        (void)m_storage->updateContactBirthday(uuid, info.birthday);

    // Проверяем аватар пира: запрашиваем если хэш изменился или нет в кэше
    const Contact stored = m_storage->getContact(uuid);
    if (!info.avatarHash.isEmpty() && info.avatarHash != stored.avatarHash) {
        // Хэш изменился — запрашиваем свежий аватар
        m_network->sendJson(uuid, QJsonObject{{"type", "AVATAR_REQUEST"}});
        qDebug("[Main] Запрос аватара у %s (хэш изменился)", qPrintable(name));
    } else if (!stored.avatarPath.isEmpty() && QFile::exists(stored.avatarPath)) {
        // Хэш совпал и файл на диске есть — показываем кэш немедленно
        qDebug("[Main] Аватар %s из кэша: %s", qPrintable(name), qPrintable(stored.avatarPath));
        if (m_activePeer == uuid)
            m_chat->setAvatar(QPixmap(stored.avatarPath));
        // Обновляем список контактов: аватар появится в иконке элемента
        m_contacts->setContacts(m_storage->allContacts());
    }
}

void MainWindow::onPeerDisconnected(QUuid uuid) {
    m_storage->updateLastSeen(uuid);
    m_contacts->setPeerOnline(uuid, false);
    // Перезагружаем контакты чтобы обновить lastSeen в списке
    const QList<Contact> updated = m_storage->allContacts();
    m_contacts->setContacts(updated);
    if (m_activePeer == uuid)
        m_chat->setPeerStatus(tr("offline"));
}

void MainWindow::onMessageReceived(QUuid from, QJsonObject msg) {
    // Игнорируем все сообщения (в т.ч. ключевой обмен) от заблокированных контактов
    {
        const Contact sender = m_storage->getContact(from);
        if (!sender.uuid.isNull() && sender.isBlocked) {
            qDebug("[Main] Сообщение от заблокированного %s — игнорируем",
                   qPrintable(from.toString(QUuid::WithoutBraces)));
            return;
        }
        // Также проверяем blocked_list — сюда попадают удалённые но заблокированные контакты
        if (sender.uuid.isNull() && m_storage->isUuidBlocked(from)) {
            qDebug("[Main] Сообщение от удалённого заблокированного %s — игнорируем",
                   qPrintable(from.toString(QUuid::WithoutBraces)));
            return;
        }
    }

    const QString type = msg["type"].toString();

    // ── Typing indicator (входящий) ───────────────────────────────────────────
    if (type == "TYPING") {
        if (m_activePeer == from) {
            const QString state = msg["state"].toString();
            if (state == "start") {
                const Contact c = m_storage->getContact(from);
                m_chat->showTypingIndicator(c.name);
                // Авто-скрытие через 5 сек если stop не пришёл
                auto*& timer = m_peerTypingTimers[from];
                if (!timer) {
                    timer = new QTimer(this);
                    timer->setSingleShot(true);
                    timer->setInterval(5000);
                    connect(timer, &QTimer::timeout, this, [this, from]() {
                        if (m_activePeer == from) m_chat->hideTypingIndicator();
                    });
                }
                timer->start();
            } else {
                m_chat->hideTypingIndicator();
                if (auto* t = m_peerTypingTimers.value(from)) t->stop();
            }
        }
        return;
    }

    // ── Статус доставки (ACK) ─────────────────────────────────────────────────
    if (type == "MSG_ACK") {
        const QString ackId = msg["msg_id"].toString();
        if (!ackId.isEmpty() && m_pendingAcks.contains(ackId)) {
            m_pendingAcks.remove(ackId);
            m_chat->markDelivered(ackId);
        }
        return;
    }

    // ── Сигналинг голосовых звонков — делегируем CallManager ─────────────────
    if (type == "CALL_INVITE" || type == "CALL_ACCEPT" ||
        type == "CALL_REJECT" || type == "CALL_END") {
        m_callManager->handleSignaling(from, msg);
        return;
    }

    // ── Сигналинг удалённого шелла — делегируем RemoteShellManager ────────────
    if (type == "SHELL_REQUEST" || type == "SHELL_ACCEPT" ||
        type == "SHELL_REJECT"  || type == "SHELL_KILL") {
        m_shellManager->handleSignaling(from, msg);
        return;
    }

    // ── Зашифрованные данные шелла (stdout/stderr или stdin) ─────────────────
    // Расшифровываем через Double Ratchet, затем передаём RemoteShellManager.
    // decrypt() игнорирует поле type — поэтому можно безопасно переиспользовать
    // механизм E2E для любого outer type.
    if (type == "SHELL_DATA" || type == "SHELL_INPUT") {
        const auto plain = m_e2e->decrypt(from, msg);
        if (plain.has_value())
            m_shellManager->handleDecryptedData(from, *plain);
        return;
    }

    if (type == "KEY_BUNDLE") {
        if (!m_e2e->hasSession(from)) {
            // Детерминированное соглашение: инициирует тот, чей UUID меньше.
            // Это гарантирует что ровно одна сторона вызовет initiateSession
            // даже при одновременном взаимном подключении (= один initiator, один responder).
            // Без этого оба вызывают initiateSession → оба становятся sender-ами
            // → несовместимые состояния ratchet → разные Safety Numbers → мусор вместо сообщений.
            const QUuid ours = Identity::instance().uuid();
            if (ours < from) {
                // Наш UUID меньше — мы инициируем X3DH-сессию
                const QJsonObject initMsg = m_e2e->initiateSession(from, msg["bundle"].toObject());
                if (!initMsg.isEmpty()) m_network->sendJson(from, initMsg);
            }
            // Иначе — наш UUID больше, ждём KEY_INIT от пира (он инициатор)
        }
        return;
    }
    if (type == "KEY_INIT") {
        const QJsonObject reply = m_e2e->acceptSession(from, msg);
        if (!reply.isEmpty()) m_network->sendJson(from, reply);
        return;
    }
    if (type == "KEY_ACK") return;

    if (type == "AVATAR_REQUEST") {
        // Проверяем уровень конфиденциальности — не отправляем аватар если запрещено
        if (!checkPrivacy(SessionManager::instance().privacyAvatar(), from)) {
            qDebug("[Main] Запрос аватара от %s отклонён (настройки конфиденциальности)",
                   qPrintable(from.toString(QUuid::WithoutBraces)));
            return;
        }

        // Пир просит наш аватар — отправляем если он есть
        const QString ownPath = SessionManager::instance().avatarPath();
        if (ownPath.isEmpty() || !QFile::exists(ownPath)) return;

        QPixmap src(ownPath);
        if (src.isNull()) return;

        // Масштабируем до 256×256 чтобы не гонять тяжёлый файл
        const QPixmap scaled = src.scaled(256, 256,
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        QByteArray pngData;
        QBuffer buf(&pngData);
        buf.open(QIODevice::WriteOnly);
        scaled.save(&buf, "PNG");
        buf.close();

        const QString hash = QString::fromLatin1(
            SessionManager::computeAvatarHash(ownPath));

        m_network->sendJson(from, QJsonObject{
            {"type", "AVATAR_DATA"},
            {"data", QString::fromLatin1(pngData.toBase64())},
            {"hash", hash},
        });
        return;
    }

    if (type == "AVATAR_DATA") {
        onAvatarDataReceived(from,
            QByteArray::fromBase64(msg["data"].toString().toLatin1()),
            msg["hash"].toString());
        return;
    }

    // ── Вторичное устройство просит отправить сообщение (мы — главное) ──────────
    if (type == "DEVICE_MSG_SEND") {
        const QUuid toUuid = QUuid(msg["to_uuid"].toString());
        const QString text = msg["text"].toString();
        const QString msgId = msg["msg_id"].toString();
        if (toUuid.isNull() || text.isEmpty()) return;
        if (!m_e2e->hasSession(toUuid)) return;

        QJsonObject env = m_e2e->encrypt(toUuid, text.toUtf8());
        if (env.isEmpty()) return;
        env["msg_id"] = msgId.isEmpty()
            ? QUuid::createUuid().toString(QUuid::WithoutBraces) : msgId;
        m_network->sendJson(toUuid, env);

        Message saved;
        saved.peerUuid  = toUuid; saved.outgoing = true;
        saved.text      = text;   saved.timestamp = QDateTime::currentDateTime();
        saved.ciphertext = QJsonDocument(env).toJson();
        (void)m_storage->saveMessage(saved);

        if (m_activePeer == toUuid) {
            m_chat->appendMessage(text, true, saved.timestamp, env["msg_id"].toString());
            m_contacts->updateLastMessage(toUuid, text);
        }

        // Уведомляем остальные вторичные устройства
        const QJsonObject relay{
            {"type",      "DEVICE_RELAY_MSG"},
            {"to_uuid",   toUuid.toString(QUuid::WithoutBraces)},
            {"msg_id",    env["msg_id"].toString()},
            {"text",      text},
            {"outgoing",  true},
            {"timestamp", saved.timestamp.toString(Qt::ISODate)},
        };
        m_network->relayToLinkedDevices(from, relay);
        return;
    }

    // ── Главное устройство прислало нам relay сообщения (мы — вторичное) ────────
    if (type == "DEVICE_RELAY_MSG") {
        const bool outgoing  = msg["outgoing"].toBool();
        const QUuid peerUuid = outgoing
            ? QUuid(msg["to_uuid"].toString())
            : QUuid(msg["from_uuid"].toString());
        const QString text   = msg["text"].toString();
        const QString msgId  = msg["msg_id"].toString();
        const QDateTime ts   = QDateTime::fromString(
            msg["timestamp"].toString(), Qt::ISODate);
        if (peerUuid.isNull() || text.isEmpty()) return;

        if (m_storage->getContact(peerUuid).uuid.isNull()) {
            const QString name = msg["from_name"].toString();
            Contact tmp; tmp.uuid = peerUuid;
            tmp.name = name.isEmpty()
                ? peerUuid.toString(QUuid::WithoutBraces).left(8) : name;
            (void)m_storage->addContact(tmp);
            m_contacts->setContacts(m_storage->allContacts());
        }

        Message saved;
        saved.peerUuid  = peerUuid; saved.outgoing = outgoing;
        saved.text      = text;
        saved.timestamp = ts.isValid() ? ts : QDateTime::currentDateTime();
        (void)m_storage->saveMessage(saved);

        if (m_activePeer == peerUuid)
            m_chat->appendMessage(text, outgoing, saved.timestamp, msgId);
        else if (!outgoing)
            m_contacts->incrementUnread(peerUuid);
        m_contacts->updateLastMessage(peerUuid, text);
        return;
    }

    if (type == "CHAT") {
        if (!checkPrivacy(SessionManager::instance().privacyMessages(), from)) {
            qDebug("[Main] Сообщение от %s отклонено (настройки конфиденциальности)",
                   qPrintable(from.toString(QUuid::WithoutBraces)));
            return;
        }

        // Авто-добавляем неизвестного отправителя — иначе saveMessage упадёт на FK-ограничении
        if (m_storage->getContact(from).uuid.isNull()) {
            const auto info = m_network->getPeerInfo(from);
            Contact tmp;
            tmp.uuid = from;
            tmp.name = info.name.isEmpty()
                ? from.toString(QUuid::WithoutBraces).left(8)
                : info.name;
            tmp.ip   = info.ip;
            tmp.port = info.serverPort;
            if (!m_storage->addContact(tmp))
                qWarning("[Main] Не удалось авто-добавить контакт %s",
                         qPrintable(tmp.name));
            m_contacts->setContacts(m_storage->allContacts());
            qDebug("[Main] Авто-добавлен неизвестный отправитель: %s", qPrintable(tmp.name));
        }

        const auto plain = m_e2e->decrypt(from, msg);
        if (!plain.has_value()) {
            // Ключи не совпадают — показываем системное сообщение вместо тихого падения
            qWarning("[Main] Ошибка расшифровки от %s",
                     qPrintable(from.toString(QUuid::WithoutBraces)));
            if (m_activePeer == from)
                m_chat->appendMessage(
                    tr("❌ Не удалось расшифровать сообщение (ключи не совпадают). "
                       "Возможна переустановка приложения у собеседника или MITM-атака."),
                    false, QDateTime::currentDateTime());
            return;
        }
        const QString text = QString::fromUtf8(*plain);

        // Отправляем ACK чтобы отправитель увидел иконку доставки
        const QString ackId = msg["msg_id"].toString();
        if (!ackId.isEmpty()) {
            m_network->sendJson(from, QJsonObject{
                {"type", "MSG_ACK"},
                {"msg_id", ackId}
            });
        }

        Message m;
        m.peerUuid = from; m.outgoing = false;
        m.text = text; m.timestamp = QDateTime::currentDateTime();
        m.ciphertext = QJsonDocument(msg).toJson();
        if (m_storage->saveMessage(m) <= 0)
            qWarning("[Main] Failed to save incoming message");

        if (m_activePeer == from) {
            m_chat->appendMessage(text, false, QDateTime::currentDateTime());
            m_chat->hideTypingIndicator();
        } else {
            m_contacts->incrementUnread(from);
            // Уведомление в трей если окно скрыто и контакт не заглушён
            const Contact sender = m_storage->getContact(from);
            if (!sender.isMuted && m_tray && (isHidden() || isMinimized())) {
                const QString senderName = sender.uuid.isNull()
                    ? from.toString(QUuid::WithoutBraces).left(8)
                    : sender.name;
                m_tray->showMessage(senderName, text,
                    QSystemTrayIcon::Information, 4000);
            }
        }
        m_contacts->updateLastMessage(from, text);

        // Пересылаем расшифрованный текст на все подключённые вторичные устройства
        {
            const Contact sender = m_storage->getContact(from);
            const QString senderName = sender.uuid.isNull()
                ? from.toString(QUuid::WithoutBraces).left(8) : sender.name;
            const QJsonObject relay{
                {"type",      "DEVICE_RELAY_MSG"},
                {"from_uuid", from.toString(QUuid::WithoutBraces)},
                {"from_name", senderName},
                {"msg_id",    ackId},
                {"text",      text},
                {"outgoing",  false},
                {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)},
            };
            m_network->relayToLinkedDevices(from, relay);
        }
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
        QMessageBox::warning(this, tr("Invalid format"),
            tr("Connection string is invalid.\nFormat: UUID@IP:Port"));
        return;
    }

    if (m_storage->getContact(peer->uuid).uuid.isNull()) {
        Contact c;
        c.uuid = peer->uuid;
        c.name = peer->uuid.toString(QUuid::WithoutBraces).left(8);
        c.ip = peer->ip;     c.port = peer->port;
        if (!m_storage->addContact(c))
            qWarning("[Main] Failed to save new contact");
        m_contacts->setContacts(m_storage->allContacts());
    }

    m_network->connectToPeer(*peer);
    statusBar()->showMessage(tr("Connecting to %1:%2...").arg(peer->ip).arg(peer->port), 4000);
}

void MainWindow::onContactSelected(QUuid uuid) {
    m_activePeer = uuid;
    m_contacts->clearUnread(uuid);
    const Contact c = m_storage->getContact(uuid);

    // Проверяем совместимость: если запись создана более новой версией приложения —
    // предупреждаем один раз за сессию (новые поля могут отображаться некорректно).
    if (!m_shownCompatWarnings.contains(uuid)) {
        const QString current = QLatin1String(UpdateChecker::kCurrentVersion);
        if (VersionUtils::isNewerThan(c.versionCreated, current)) {
            m_shownCompatWarnings.insert(uuid);
            QMessageBox::information(this, tr("Предупреждение совместимости"),
                tr("Контакт <b>%1</b> использует более новую версию приложения (v%2).<br>"
                   "Некоторые данные могут отображаться некорректно.<br>"
                   "Обновите приложение до последней версии.")
                .arg(c.name, c.versionCreated));
        }
    }

    m_chat->openConversation(c.name, m_network->isOnline(uuid));

    // Предобработка истории: заменяем текст сообщений из будущих версий заглушкой
    QList<Message> msgs = m_storage->getMessages(uuid, 50);
    const QString current = QLatin1String(UpdateChecker::kCurrentVersion);
    for (auto& msg : msgs) {
        if (!msg.isVoice && VersionUtils::isNewerThan(msg.versionCreated, current))
            msg.text = tr("[Сообщение из более новой версии — обновите приложение]");
    }
    m_chat->loadHistory(msgs);

    // Передаём UUID для сигнала openProfileRequested (клик по аватару)
    m_chat->setPeerUuid(uuid);
    // Загружаем кэшированный аватар если есть
    if (!c.avatarPath.isEmpty() && QFile::exists(c.avatarPath))
        m_chat->setAvatar(QPixmap(c.avatarPath));
    else
        m_chat->setAvatar({});
}

void MainWindow::onSendMessage(const QString& text) {
    if (m_activePeer.isNull() || text.trimmed().isEmpty()) return;

    // Если мы вторичное устройство — пересылаем через главное
    const QUuid primaryUuid = m_network->primaryDeviceUuid();
    if (!primaryUuid.isNull()) {
        const QString msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_network->sendJson(primaryUuid, QJsonObject{
            {"type",    "DEVICE_MSG_SEND"},
            {"to_uuid", m_activePeer.toString(QUuid::WithoutBraces)},
            {"msg_id",  msgId},
            {"text",    text},
        });
        Message msg;
        msg.peerUuid  = m_activePeer; msg.outgoing = true;
        msg.text      = text; msg.timestamp = QDateTime::currentDateTime();
        (void)m_storage->saveMessage(msg);
        m_chat->appendMessage(text, true, msg.timestamp, msgId);
        m_contacts->updateLastMessage(m_activePeer, text);
        return;
    }

    if (!m_e2e->hasSession(m_activePeer)) {
        QMessageBox::warning(this, tr("E2E not ready"),
            tr("Encryption session is not established yet. Wait a moment."));
        return;
    }

    const QString msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject env = m_e2e->encrypt(m_activePeer, text.toUtf8());
    if (env.isEmpty()) return;
    env["msg_id"] = msgId;
    m_network->sendJson(m_activePeer, env);
    m_pendingAcks[msgId] = m_activePeer;

    Message msg;
    msg.peerUuid = m_activePeer; msg.outgoing = true;
    msg.text = text; msg.timestamp = QDateTime::currentDateTime();
    msg.ciphertext = QJsonDocument(env).toJson();
    if (m_storage->saveMessage(msg) <= 0)
        qWarning("[Main] Failed to save outgoing message");

    m_chat->appendMessage(text, true, QDateTime::currentDateTime(), msgId);
    m_contacts->updateLastMessage(m_activePeer, text);
}

void MainWindow::onSendFile() {
    if (m_activePeer.isNull()) return;

    // DontUseNativeDialog: обязателен в AppImage/кастомных сборках.
    // Нативный диалог может крашить приложение при отсутствии системных
    // плагинов Qt (platformthemes, libxcb и т.д.).
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Выбрать файл для отправки"),
        QString(),   // стартовая директория: последняя использованная
        QString(),   // фильтр: все файлы
        nullptr,     // selectedFilter
        QFileDialog::DontUseNativeDialog);

    if (path.isEmpty()) return;

    qDebug("[Main] Отправка файла: %s", qPrintable(path));
    m_fileTransfer->sendFile(m_activePeer, path);
}

void MainWindow::onSendVoice(const QString& filePath, int durationMs) {
    if (m_activePeer.isNull() || filePath.isEmpty()) return;

    // Отправляем через FileTransfer с меткой голосового (durationMs > 0)
    m_fileTransfer->sendFile(m_activePeer, filePath, durationMs);

    // Показываем исходящий голосовой пузырь немедленно
    const QDateTime now = QDateTime::currentDateTime();
    m_chat->appendVoiceMessage(true, durationMs, now, filePath);

    // Сохраняем в историю (filePath — в поле text, is_voice=true)
    Message msg;
    msg.peerUuid        = m_activePeer;
    msg.outgoing        = true;
    msg.isVoice         = true;
    msg.voiceDurationMs = durationMs;
    msg.text            = filePath;
    msg.timestamp       = now;
    if (m_storage->saveMessage(msg) <= 0)
        qWarning("[Main] Не удалось сохранить исходящее голосовое");
}

// ── Голосовые звонки ─────────────────────────────────────────────────────────

void MainWindow::onCallRequested(QUuid peerUuid) {
    if (m_callManager->state() != CallManager::CallState::Idle) {
        QMessageBox::information(this, "Звонок", "Уже идёт звонок");
        return;
    }
    const auto info = m_network->getPeerInfo(peerUuid);
    if (info.state != ConnectionState::Connected) {
        QMessageBox::warning(this, "Звонок", "Пир не подключён");
        return;
    }

    m_callWindow = new CallWindow(this);
    m_callWindow->setPeerName(info.name);
    m_callWindow->setState(CallWindow::State::Calling);
    connect(m_callWindow, &CallWindow::muteToggled,
            m_callManager->mediaEngine(), &MediaEngine::setMuted);
    connect(m_callWindow, &CallWindow::hangupClicked,
            m_callManager, &CallManager::endCall);
    connect(m_callManager->mediaEngine(), &MediaEngine::audioLevelChanged,
            m_callWindow, &CallWindow::setAudioLevel);
#ifdef HAVE_QT_MULTIMEDIA
    connect(m_callWindow, &CallWindow::inputDeviceChanged,
            m_callManager->mediaEngine(), &MediaEngine::setInputDevice);
    connect(m_callWindow, &CallWindow::outputDeviceChanged,
            m_callManager->mediaEngine(), &MediaEngine::setOutputDevice);
#endif
    m_callWindow->show();

    m_callManager->initiateCall(peerUuid, QHostAddress(info.ip));
}

void MainWindow::onIncomingCall(QUuid from, QString callerName, QString callId) {
    if (!checkPrivacy(SessionManager::instance().privacyCalls(), from)) {
        qDebug("[Main] Входящий звонок от %s отклонён (настройки конфиденциальности)",
               qPrintable(from.toString(QUuid::WithoutBraces)));
        m_callManager->rejectCall(callId);
        return;
    }

    m_callWindow = new CallWindow(this);
    m_callWindow->setPeerName(callerName.isEmpty() ? from.toString() : callerName);
    m_callWindow->setState(CallWindow::State::Ringing);
    connect(m_callWindow, &CallWindow::muteToggled,
            m_callManager->mediaEngine(), &MediaEngine::setMuted);
    connect(m_callWindow, &CallWindow::hangupClicked,
            m_callManager, &CallManager::endCall);
    connect(m_callWindow, &CallWindow::acceptClicked, this, [this, callId]() {
        m_callManager->acceptCall(callId);
    });
    connect(m_callWindow, &CallWindow::rejectClicked, this, [this, callId]() {
        m_callManager->rejectCall(callId);
        if (m_callWindow) {
            m_callWindow->hide();
            m_callWindow->deleteLater();
            m_callWindow = nullptr;
        }
    });
    connect(m_callManager->mediaEngine(), &MediaEngine::audioLevelChanged,
            m_callWindow, &CallWindow::setAudioLevel);
#ifdef HAVE_QT_MULTIMEDIA
    connect(m_callWindow, &CallWindow::inputDeviceChanged,
            m_callManager->mediaEngine(), &MediaEngine::setInputDevice);
    connect(m_callWindow, &CallWindow::outputDeviceChanged,
            m_callManager->mediaEngine(), &MediaEngine::setOutputDevice);
#endif
    m_callWindow->show();
}

void MainWindow::onCallEnded(QUuid /*peer*/) {
    if (m_callWindow) {
        m_callWindow->hide();
        m_callWindow->deleteLater();
        m_callWindow = nullptr;
    }
}

void MainWindow::onShowMyId() {
    const auto& id = Identity::instance();
    const QString ip   = m_network->externalIp();
    const quint16 port = m_network->advertisedPort();
    const QString connStr = id.connectionString(ip, port);

    QMessageBox dlg(this);
    dlg.setWindowTitle(tr("My ID"));

    if (ip.isEmpty()) {
        dlg.setText(tr("<b>Warning: public IP not yet known</b>"));
        dlg.setInformativeText(
            tr("The app is still discovering your external IP.\n"
               "Wait a few seconds and try again, or set Manual mode in Settings.\n\n"
               "Incomplete string (not usable yet):\n%1").arg(connStr));
    } else {
        dlg.setText(tr("<b>Send this string to your contact:</b>"));
        dlg.setInformativeText(connStr);
    }

    auto* copyBtn = dlg.addButton(tr("Copy"), QMessageBox::ActionRole);
    dlg.addButton(tr("Close"), QMessageBox::RejectRole);
    dlg.exec();
    if (dlg.clickedButton() == copyBtn) {
        QApplication::clipboard()->setText(connStr);
        statusBar()->showMessage(tr("Copied!"), 2000);
    }
}

void MainWindow::onEditName() {
    bool ok;
    const QString name = QInputDialog::getText(
        this, tr("Edit name"), tr("New name:"),
        QLineEdit::Normal, Identity::instance().displayName(), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        Identity::instance().setDisplayName(name.trimmed());
    }
}

void MainWindow::onAvatarDataReceived(QUuid from,
                                       const QByteArray& pngData,
                                       const QString& hash) {
    if (pngData.isEmpty() || hash.isEmpty()) return;

    // Сохраняем аватар в ~/.cache/naleystogramm/avatars/{hash}.png
    QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                  + "/avatars");
    cacheDir.mkpath(".");
    const QString savePath = cacheDir.filePath(hash + ".png");

    QFile f(savePath);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning("[Main] Не удалось сохранить аватар %s: %s",
                 qPrintable(savePath), qPrintable(f.errorString()));
        return;
    }
    f.write(pngData);
    f.close();

    if (!m_storage->updateAvatar(from, hash, savePath))
        qWarning("[Main] Не удалось обновить аватар в БД для %s",
                 qPrintable(from.toString(QUuid::WithoutBraces)));
    qDebug("[Main] Аватар сохранён: %s", qPrintable(savePath));

    // Обновляем шапку чата если этот пир сейчас активен
    if (m_activePeer == from)
        m_chat->setAvatar(QPixmap(savePath));

    // Обновляем список контактов — иконка аватара появится сразу у нужного элемента
    m_contacts->setContacts(m_storage->allContacts());
}

void MainWindow::onContactNameUpdated(QUuid uuid, QString name) {
    // Игнорируем неизвестных пиров
    Contact c = m_storage->getContact(uuid);
    if (c.uuid.isNull()) return;

    if (c.name == name) return;  // Имя не изменилось

    // Обновляем базу данных
    if (!m_storage->updateContactName(uuid, name)) {
        qWarning("[Main] Не удалось обновить имя контакта %s",
                 qPrintable(uuid.toString(QUuid::WithoutBraces)));
    }

    qDebug("[Main] Имя контакта обновлено: \"%s\" → \"%s\"",
           qPrintable(c.name), qPrintable(name));

    // Обновляем список контактов
    m_contacts->updateContactName(uuid, name);

    // Обновляем шапку чата если это активный пир
    if (m_activePeer == uuid)
        m_chat->setPeerName(name);
}

void MainWindow::onBlockContact(QUuid uuid) {
    const Contact c = m_storage->getContact(uuid);
    if (c.uuid.isNull()) return;

    const bool newState = !c.isBlocked; // переключаем флаг
    if (!m_storage->blockContact(uuid, newState)) {
        qWarning("[Main] Не удалось изменить блокировку для %s",
                 qPrintable(uuid.toString(QUuid::WithoutBraces)));
        return;
    }

    // Перезагружаем список — цвет имени изменится в rebuildList
    m_contacts->setContacts(m_storage->allContacts());

    const QString action = newState ? tr("заблокирован") : tr("разблокирован");
    statusBar()->showMessage(tr("%1 %2").arg(c.name, action), 4000);
    qDebug("[Main] Контакт %s %s", qPrintable(c.name), qPrintable(action));
}

void MainWindow::onDeleteChat(QUuid uuid) {
    const Contact c = m_storage->getContact(uuid);
    if (c.uuid.isNull()) return;

    const auto btn = QMessageBox::question(
        this,
        tr("Удалить чат"),
        tr("Удалить всю переписку с %1?\nСам контакт останется в списке.").arg(c.name),
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;

    if (!m_storage->clearMessages(uuid))
        qWarning("[Main] Не удалось очистить переписку с %s",
                 qPrintable(uuid.toString(QUuid::WithoutBraces)));

    // Очищаем окно чата если это активный пир
    if (m_activePeer == uuid)
        m_chat->loadHistory({});

    // Убираем превью последнего сообщения
    m_contacts->updateLastMessage(uuid, {});

    statusBar()->showMessage(tr("Чат с %1 удалён").arg(c.name), 4000);
    qDebug("[Main] Переписка с %s удалена", qPrintable(c.name));
}

void MainWindow::onDeleteContact(QUuid uuid) {
    const Contact c = m_storage->getContact(uuid);
    if (c.uuid.isNull()) return;

    // Подсказка о блокировке в тексте диалога — пользователь видит что произойдёт с блокировкой
    const QString blockedHint = c.isBlocked
        ? tr("\nКонтакт будет добавлен в список блокировки — новые сообщения от него будут отклонены.")
        : QString();

    const auto btn = QMessageBox::question(
        this,
        tr("Удалить контакт"),
        tr("Удалить контакт %1 и всю переписку?%2\n\nЭто действие необратимо.")
            .arg(c.name, blockedHint),
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;

    // deleteContact удаляет переписку, сам контакт и (если был заблокирован) пишет в blocked_list
    if (!m_storage->deleteContact(uuid)) {
        qWarning("[Main] Не удалось удалить контакт %s",
                 qPrintable(uuid.toString(QUuid::WithoutBraces)));
        return;
    }

    // Если это был активный пир — сбрасываем чат
    if (m_activePeer == uuid) {
        m_activePeer = QUuid();
        m_chat->showPlaceholder();
    }

    // Обновляем список контактов
    m_contacts->setContacts(m_storage->allContacts());

    statusBar()->showMessage(tr("Контакт %1 удалён").arg(c.name), 4000);
    qDebug("[Main] Контакт %s удалён", qPrintable(c.name));
}

void MainWindow::onOpenProfile(QUuid uuid) {
    // Избегаем дублирующих окон для одного пира
    // Уже открыт для того же UUID — поднимаем поверх
    const auto dialogs = findChildren<ContactProfileDialog*>();
    for (auto* dlg : dialogs) {
        if (dlg->property("peerUuid").toUuid() == uuid) {
            dlg->openPanel();
            return;
        }
    }

    auto* dlg = new ContactProfileDialog(uuid, m_network, m_storage, this);
    dlg->setProperty("peerUuid", uuid);

    // Устанавливаем номер безопасности (Safety Number) если сессия уже установлена
    dlg->setSafetyNumber(m_e2e->getSafetyNumber(uuid));

    // Обновляем виджет когда пришла новая информация о пире
    connect(m_network, &NetworkManager::peerInfoUpdated,
            dlg, [dlg, uuid](const QUuid& u) {
                if (u == uuid) dlg->refreshData();
            });

    // Кнопки действий в профиле
    connect(dlg, &ContactProfileDialog::shellRequested,
            this, &MainWindow::onShellRequestedFromProfile);
    connect(dlg, &ContactProfileDialog::callRequested,
            this, &MainWindow::onCallRequested);
    connect(dlg, &ContactProfileDialog::blockRequested,
            this, &MainWindow::onBlockContact);

    dlg->openPanel();
}

// ── Удалённый шелл ────────────────────────────────────────────────────────────

// Кнопка ">_" нажата в диалоге профиля — проверяем E2E и запрашиваем шелл
void MainWindow::onShellRequestedFromProfile(QUuid peerUuid) {
    if (!m_e2e->hasSession(peerUuid)) {
        QMessageBox::warning(this, tr("Шелл недоступен"),
            tr("E2E-сессия с контактом ещё не установлена.\n"
               "Подождите несколько секунд и повторите."));
        return;
    }
    if (!m_network->isOnline(peerUuid)) {
        QMessageBox::warning(this, tr("Шелл недоступен"),
            tr("Контакт не в сети."));
        return;
    }
    m_shellManager->requestShell(peerUuid);
    statusBar()->showMessage(tr("Запрос шелла отправлен..."), 4000);
}

// Receiver: автоматически сгенерирован OTP — показываем пароль в диалоге
void MainWindow::onShellChallengeGenerated(QString sessionId, QUuid from,
                                            QString peerName, QString otp) {
    if (!SessionManager::instance().remoteShellEnabled()) {
        qWarning("[Shell] Входящий запрос отклонён: удалённый шелл отключён в настройках");
        m_shellManager->rejectRequest(sessionId, "disabled_by_user");
        return;
    }
    if (!checkPrivacy(SessionManager::instance().privacyShell(), from)) {
        qDebug("[Shell] Входящий запрос от %s отклонён (настройки конфиденциальности)",
               qPrintable(from.toString(QUuid::WithoutBraces)));
        m_shellManager->rejectRequest(sessionId, "privacy");
        return;
    }

    const QString name = peerName.isEmpty()
        ? from.toString(QUuid::WithoutBraces).left(8)
        : peerName;

    // Создаём монитор заранее — данные могут прийти сразу после спауна процесса
    auto* monitor = new ShellMonitor(sessionId, name, this);
    m_shellMonitors.insert(sessionId, monitor);
    connect(monitor, &ShellMonitor::terminateRequested,
            this, [this](const QString& sid) {
        m_shellManager->killSession(sid, "receiver_terminated");
    });

    // Показываем OTP пользователю — только кнопка «Отклонить»
    auto* box = new QMessageBox(
        QMessageBox::Warning,
        tr("Запрос удалённого шелла"),
        tr("Контакт <b>%1</b> запрашивает шелл-сессию на вашем компьютере.<br><br>"
           "Ваш одноразовый пароль:<br>"
           "<div align='center'><b><tt style='font-size:18pt'>%2</tt></b></div><br>"
           "Сообщите этот пароль контакту вне чата,<br>"
           "если хотите разрешить доступ.<br>"
           "Вы сможете видеть все команды и завершить сессию в любой момент.").arg(name, otp),
        QMessageBox::Cancel,
        this);
    box->button(QMessageBox::Cancel)->setText(tr("Отклонить"));
    box->setAttribute(Qt::WA_DeleteOnClose);

    connect(box, &QMessageBox::finished, this, [this, sessionId, monitor, box](int result) {
        if (result == QMessageBox::Cancel) {
            // Пользователь явно нажал «Отклонить»
            m_shellMonitors.remove(sessionId);
            monitor->deleteLater();
            m_shellManager->rejectRequest(sessionId, "declined");
        }
        // Если диалог закрылся иначе (shellSessionStarted) — сессия уже работает
    });

    // Автозакрытие диалога когда шелл запущен (пароль принят инициатором)
    connect(m_shellManager, &RemoteShellManager::shellSessionStarted,
            box, [box, sessionId](const QString& sid) {
        if (sid == sessionId) box->done(QMessageBox::Ok);
    });
    // Автозакрытие если сессия завершилась до принятия пароля
    connect(m_shellManager, &RemoteShellManager::sessionEnded,
            box, [box, sessionId](const QString& sid, const QString&) {
        if (sid == sessionId) box->done(QMessageBox::Ok);
    });

    box->show();
    statusBar()->showMessage(tr("Входящий запрос шелла от %1").arg(name), 6000);
}

// Receiver: шелл-процесс успешно запущен — показываем монитор
void MainWindow::onShellSessionStarted(QString sessionId) {
    auto it = m_shellMonitors.find(sessionId);
    if (it == m_shellMonitors.end()) return;
    it.value()->show();
    statusBar()->showMessage(tr("Шелл-сессия открыта"), 4000);
}

// Initiator: пир ждёт ввода OTP — показываем диалог ввода пароля
void MainWindow::onShellPasswordRequired(QString sessionId, QUuid /*peerUuid*/,
                                          QString peerName) {
    const QString name = peerName.isEmpty() ? tr("контакт") : peerName;
    bool ok = false;
    const QString password = QInputDialog::getText(
        this,
        tr("Подтверждение шелла"),
        tr("Введите одноразовый пароль, который отображается в окне <b>%1</b>:").arg(name),
        QLineEdit::Normal,
        QString{},
        &ok);

    if (!ok || password.trimmed().isEmpty()) {
        m_shellManager->killSession(sessionId, "initiator_cancelled");
        return;
    }

    m_shellManager->respondToChallenge(sessionId, password.trimmed().toUpper());
}

// Инициатор: пир принял запрос — открываем ShellWindow
void MainWindow::onShellAccepted(QString sessionId, QUuid /*peerUuid*/, QString peerName) {
    const QString name = peerName.isEmpty() ? tr("контакт") : peerName;

    auto* win = new ShellWindow(sessionId, name, this);
    m_shellWindows.insert(sessionId, win);

    connect(win, &ShellWindow::inputSubmitted,
            this, [this](const QString& sid, const QByteArray& data) {
        m_shellManager->sendInput(sid, data);
    });
    connect(win, &ShellWindow::terminateRequested,
            this, [this](const QString& sid) {
        m_shellManager->killSession(sid, "initiator_terminated");
    });
    win->show();
    statusBar()->showMessage(tr("Шелл-сессия с %1 открыта").arg(name), 4000);
}

// Инициатор: пир отклонил запрос
void MainWindow::onShellRejected(QString sessionId, QString reason) {
    Q_UNUSED(sessionId)
    QString msg;
    if (reason == "declined")
        msg = tr("Контакт отклонил запрос шелла.");
    else if (reason == "wrong_password")
        msg = tr("Неверный одноразовый пароль — шелл-сессия закрыта.");
    else if (reason == "busy")
        msg = tr("У контакта уже открыта другая шелл-сессия.");
    else if (reason == "max_sessions")
        msg = tr("Невозможно открыть шелл: уже есть активная сессия.");
    else if (reason == "disabled_by_user")
        msg = tr("Контакт отключил удалённый шелл в настройках.");
    else
        msg = tr("Запрос шелла отклонён: %1").arg(reason);
    QMessageBox::information(this, tr("Шелл отклонён"), msg);
}

// Данные stdout/stderr от удалённого шелла → показать в ShellWindow
void MainWindow::onShellDataReceived(QString sessionId, QByteArray data) {
    auto* win = m_shellWindows.value(sessionId, nullptr);
    if (win) win->appendOutput(data);
}

// Команда от инициатора (видит только получатель в ShellMonitor)
void MainWindow::onInputMonitored(QString sessionId, QByteArray data) {
    auto* monitor = m_shellMonitors.value(sessionId, nullptr);
    if (monitor) monitor->appendInput(data);
}

// Сессия завершена — закрываем окна обеих сторон и очищаем хэши
void MainWindow::onShellSessionEnded(QString sessionId, QString reason) {
    if (auto* win = m_shellWindows.value(sessionId, nullptr)) {
        win->showSessionEnded(reason);
        // Даём пользователю прочитать сообщение — не закрываем принудительно
    }
    m_shellWindows.remove(sessionId);

    if (auto* monitor = m_shellMonitors.value(sessionId, nullptr)) {
        monitor->showSessionEnded(reason);
    }
    m_shellMonitors.remove(sessionId);

    if (reason == "privilege_escalation")
        statusBar()->showMessage(
            tr("ШЕЛЛ УБИТ: попытка эскалации привилегий!"), 8000);
    else
        statusBar()->showMessage(tr("Шелл-сессия завершена"), 4000);
}

// Обнаружена попытка эскалации — предупреждаем инициатора явным диалогом
void MainWindow::onPrivilegeEscalationDetected(QString sessionId) {
    Q_UNUSED(sessionId)
    QMessageBox::critical(this, tr("Эскалация привилегий!"),
        tr("Обнаружена попытка эскалации привилегий!\n\n"
           "Команды sudo / su / pkexec / runas / gsudo запрещены.\n"
           "Шелл-сессия была немедленно уничтожена."));
}
