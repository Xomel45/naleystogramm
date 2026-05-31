#include "settingspage_plugins.h"
#include "settingshelpers.h"
#include "../thememanager.h"
#include "../toggleswitch.h"
#include "../../plugins/pluginmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>

SettingsPluginsPage::SettingsPluginsPage(QWidget* parent)
    : SettingsPageBase(parent)
{
    // ── Заголовок секции ──────────────────────────────────────────────────
    auto* titleRow = new QWidget();
    auto* trl = new QHBoxLayout(titleRow);
    trl->setContentsMargins(0, 0, 0, 4);
    trl->setSpacing(8);

    auto* sectionLbl = new QLabel(tr("Установленные плагины"));
    sectionLbl->setObjectName("settingsFieldLabel");

    auto* reloadBtn = new QPushButton(tr("Перезагрузить"));
    reloadBtn->setObjectName("dlgOkBtn");
    reloadBtn->setFixedHeight(28);
    connect(reloadBtn, &QPushButton::clicked, this, [this]() {
        PluginManager::instance().reload();
    });

    trl->addWidget(sectionLbl, 1);
    trl->addWidget(reloadBtn);
    m_lay->addWidget(titleRow);

    // ── Список плагинов ───────────────────────────────────────────────────
    auto* listWidget = new QWidget();
    m_listLay = new QVBoxLayout(listWidget);
    m_listLay->setContentsMargins(0, 0, 0, 0);
    m_listLay->setSpacing(0);
    m_lay->addWidget(listWidget);

    m_emptyLbl = new QLabel(tr("Плагины не установлены"));
    m_emptyLbl->setObjectName("settingsHint");
    m_emptyLbl->setAlignment(Qt::AlignCenter);
    m_emptyLbl->setWordWrap(true);
    m_lay->addWidget(m_emptyLbl);

    m_lay->addWidget(spSeparator());
    m_lay->addWidget(spHint(
        tr("Плагины — .plugin файлы (архив с .so/.dll).\n"
           "Поместите их в папку плагинов и нажмите «Перезагрузить».")));

    // ── Кнопка «Открыть папку» ────────────────────────────────────────────
    m_lay->addSpacing(8);
    auto* folderBtn = new QPushButton();
    folderBtn->setObjectName("settingsNavRow");
    folderBtn->setFixedHeight(44);
    folderBtn->setCursor(Qt::PointingHandCursor);
    connect(folderBtn, &QPushButton::clicked, this, &SettingsPluginsPage::openPluginsFolder);

    auto* fbl = new QHBoxLayout(folderBtn);
    fbl->setContentsMargins(16, 0, 16, 0);
    fbl->setSpacing(10);

    auto* folderIco = new QLabel();
    folderIco->setPixmap(
        ThemeManager::tintedIcon(QStringLiteral(":/icons/nav_attach.png")).pixmap(18, 18));
    folderIco->setFixedSize(24, 24);
    folderIco->setAlignment(Qt::AlignCenter);
    folderIco->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* folderLbl = new QLabel(tr("Открыть папку плагинов"));
    folderLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

    fbl->addWidget(folderIco);
    fbl->addWidget(folderLbl, 1);
    m_lay->addWidget(folderBtn);
    m_lay->addStretch();

    rebuildList();

    connect(&PluginManager::instance(), &PluginManager::pluginsChanged,
            this, &SettingsPluginsPage::rebuildList);
}

void SettingsPluginsPage::reload() {
    rebuildList();
}

// ── Построение списка ─────────────────────────────────────────────────────

void SettingsPluginsPage::rebuildList() {
    QLayoutItem* item;
    while ((item = m_listLay->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const auto entries = PluginManager::instance().plugins();
    m_emptyLbl->setVisible(entries.isEmpty());

    for (const auto& e : entries) {
        auto* row = new QWidget();
        row->setObjectName("settingsNavRow");
        row->setMinimumHeight(68);

        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(16, 10, 16, 10);
        hl->setSpacing(12);

        // ── Иконка статуса ────────────────────────────────────────────────
        auto* ico = new QLabel();
        ico->setFixedSize(28, 28);
        ico->setAlignment(Qt::AlignCenter);

        switch (e.state) {
        case PluginState::Locked:
            ico->setPixmap(
                ThemeManager::tintedIcon(QStringLiteral(":/icons/dialogs_lock_on.png"))
                .pixmap(20, 20));
            break;
        case PluginState::Error:
            ico->setPixmap(
                ThemeManager::tintedIcon(QStringLiteral(":/icons/ctx_cancel.png"))
                .pixmap(20, 20));
            break;
        default:
            ico->setPixmap(
                ThemeManager::tintedIcon(QStringLiteral(":/icons/settings_advanced.png"))
                .pixmap(20, 20));
            break;
        }

        // ── Название + описание + версия ──────────────────────────────────
        auto* infoCol = new QVBoxLayout();
        infoCol->setSpacing(2);
        infoCol->setContentsMargins(0, 0, 0, 0);

        auto* nameLbl = new QLabel(e.meta.name);
        nameLbl->setObjectName("settingsNavRowText");

        QString statusStr;
        switch (e.state) {
        case PluginState::Locked: statusStr = tr("🔒 Зашифрован — требуется ключ"); break;
        case PluginState::Error:  statusStr = tr("⚠ Ошибка загрузки");             break;
        default: statusStr = e.meta.description;                                    break;
        }

        auto* descLbl = new QLabel(
            statusStr + QStringLiteral("  <small>v") + e.meta.version + QStringLiteral("</small>"));
        descLbl->setObjectName("settingsHint");
        descLbl->setTextFormat(Qt::RichText);
        descLbl->setWordWrap(true);

        if (!e.meta.author.isEmpty()) {
            auto* authorLbl = new QLabel(tr("Автор: ") + e.meta.author);
            authorLbl->setObjectName("settingsHint");
            infoCol->addWidget(nameLbl);
            infoCol->addWidget(descLbl);
            infoCol->addWidget(authorLbl);
        } else {
            infoCol->addWidget(nameLbl);
            infoCol->addWidget(descLbl);
        }

        hl->addWidget(ico);
        hl->addLayout(infoCol, 1);

        // ── Кнопки справа ─────────────────────────────────────────────────
        if (e.state == PluginState::Locked) {
            // Кнопка ввода ключа
            auto* keyBtn = new QPushButton(tr("Ввести ключ"));
            keyBtn->setObjectName("dlgOkBtn");
            keyBtn->setFixedHeight(28);
            const QString pluginId = e.meta.id;
            const QString pluginName = e.meta.name;
            connect(keyBtn, &QPushButton::clicked, this, [pluginId, pluginName, this]() {
                showKeyDialog(pluginId, pluginName);
            });
            hl->addWidget(keyBtn);

        } else if (e.state != PluginState::Error) {
            // Кнопка настроек плагина (если есть страница)
            if (e.plugin) {
                QWidget* testPage = e.plugin->createSettingsPage();
                if (testPage) {
                    delete testPage;  // сразу удаляем тест-экземпляр
                    auto* cfgBtn = new QPushButton();
                    cfgBtn->setObjectName("iconBtn");
                    cfgBtn->setFixedSize(30, 30);
                    cfgBtn->setToolTip(tr("Настройки плагина"));
                    ThemeManager::applyIcon(cfgBtn,
                        QStringLiteral(":/icons/settings_advanced.png"), QSize(16, 16));
                    IPlugin* pluginPtr = e.plugin;
                    connect(cfgBtn, &QPushButton::clicked, this, [pluginPtr, this]() {
                        showPluginSettings(pluginPtr);
                    });
                    hl->addWidget(cfgBtn);
                }
            }

            // Тогл вкл/выкл
            auto* tog = new ToggleSwitch();
            tog->setChecked(e.enabled);
            const QString pluginId = e.meta.id;
            connect(tog, &ToggleSwitch::toggled, this, [pluginId](bool on) {
                PluginManager::instance().setEnabled(pluginId, on);
            });
            hl->addWidget(tog);
        }

        m_listLay->addWidget(row);
        m_listLay->addWidget(spSeparator());
    }
}

// ── Диалог ввода ключа ────────────────────────────────────────────────────

void SettingsPluginsPage::showKeyDialog(const QString& id, const QString& name) {
    auto* dlg = new QDialog(window());
    dlg->setWindowTitle(tr("Ключ плагина"));
    dlg->setFixedWidth(380);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto* dl = new QVBoxLayout(dlg);
    dl->setSpacing(12);
    dl->setContentsMargins(20, 20, 20, 20);

    auto* lbl = new QLabel(tr("Введите ключ для плагина <b>%1</b>:").arg(name));
    lbl->setWordWrap(true);

    auto* edit = new QLineEdit();
    edit->setEchoMode(QLineEdit::Password);
    edit->setPlaceholderText(tr("Ключ доступа..."));

    auto* btnRow = new QWidget();
    auto* brl = new QHBoxLayout(btnRow);
    brl->setContentsMargins(0, 0, 0, 0);
    brl->setSpacing(8);

    auto* cancelBtn = new QPushButton(tr("Отмена"));
    cancelBtn->setObjectName("dlgCancelBtn");
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

    auto* okBtn = new QPushButton(tr("Разблокировать"));
    okBtn->setObjectName("dlgOkBtn");
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, dlg, [dlg, edit, id, this]() {
        const QString key = edit->text().trimmed();
        if (key.isEmpty()) return;
        if (PluginManager::instance().unlock(id, key)) {
            dlg->accept();
        } else {
            QMessageBox::warning(dlg, tr("Неверный ключ"),
                tr("Ключ неверный или файл плагина повреждён.\n"
                   "Пожалуйста, проверьте ключ и попробуйте снова."));
        }
    });

    brl->addStretch();
    brl->addWidget(cancelBtn);
    brl->addWidget(okBtn);

    dl->addWidget(lbl);
    dl->addWidget(edit);
    dl->addWidget(btnRow);

    edit->setFocus();
    dlg->exec();
}

// ── Диалог настроек плагина ───────────────────────────────────────────────

void SettingsPluginsPage::showPluginSettings(IPlugin* plugin) {
    QWidget* page = plugin->createSettingsPage();
    if (!page) return;

    auto* dlg = new QDialog(window());
    dlg->setWindowTitle(plugin->settingsPageTitle());
    dlg->setMinimumSize(480, 400);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto* dl = new QVBoxLayout(dlg);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(0);

    auto* hdr = new QWidget();
    hdr->setObjectName("settingsCardHeader");
    hdr->setFixedHeight(52);
    auto* hdrl = new QHBoxLayout(hdr);
    hdrl->setContentsMargins(16, 0, 12, 0);

    auto* titleLbl = new QLabel(plugin->settingsPageTitle());
    titleLbl->setObjectName("settingsCardTitle");

    auto* closeBtn = new QPushButton();
    closeBtn->setObjectName("iconBtn");
    closeBtn->setFixedSize(30, 30);
    ThemeManager::applyIcon(closeBtn,
        QStringLiteral(":/icons/ctx_cancel.png"), QSize(16, 16));
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    hdrl->addWidget(titleLbl, 1);
    hdrl->addWidget(closeBtn);

    page->setParent(dlg);
    dl->addWidget(hdr);
    dl->addWidget(page, 1);
    dlg->exec();
}

void SettingsPluginsPage::openPluginsFolder() const {
    const QString dir = PluginManager::pluginsDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
