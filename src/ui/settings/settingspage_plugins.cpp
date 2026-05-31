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
        rebuildList();
    });

    trl->addWidget(sectionLbl, 1);
    trl->addWidget(reloadBtn);
    m_lay->addWidget(titleRow);

    // ── Список плагинов (динамически заполняется) ─────────────────────────
    auto* listWidget = new QWidget();
    m_listLay = new QVBoxLayout(listWidget);
    m_listLay->setContentsMargins(0, 0, 0, 0);
    m_listLay->setSpacing(0);
    m_lay->addWidget(listWidget);

    // ── Сообщение при пустом списке ───────────────────────────────────────
    m_emptyLbl = new QLabel(tr("Плагины не установлены"));
    m_emptyLbl->setObjectName("settingsHint");
    m_emptyLbl->setAlignment(Qt::AlignCenter);
    m_emptyLbl->setWordWrap(true);
    m_lay->addWidget(m_emptyLbl);

    m_lay->addWidget(spSeparator());

    // ── Подсказка: куда класть плагины ───────────────────────────────────
    auto* hint = spHint(
        tr("Плагины — это .so (Linux) или .dll (Windows) файлы.\n"
           "Поместите их в папку плагинов и нажмите «Перезагрузить»."));
    m_lay->addWidget(hint);

    // ── Кнопка «Открыть папку» ────────────────────────────────────────────
    m_lay->addSpacing(8);
    auto* folderBtn = new QPushButton(tr("Открыть папку плагинов"));
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

    // Обновляемся при изменении плагинов (reload)
    connect(&PluginManager::instance(), &PluginManager::pluginsChanged,
            this, &SettingsPluginsPage::rebuildList);
}

void SettingsPluginsPage::reload() {
    rebuildList();
}

void SettingsPluginsPage::rebuildList() {
    // Удаляем все текущие строки плагинов
    QLayoutItem* item;
    while ((item = m_listLay->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const auto entries = PluginManager::instance().plugins();
    m_emptyLbl->setVisible(entries.isEmpty());

    for (const auto& e : entries) {
        if (!e.plugin) continue;

        // ── Строка плагина ─────────────────────────────────────────────────
        auto* row = new QWidget();
        row->setObjectName("settingsNavRow");
        row->setMinimumHeight(64);

        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(16, 10, 16, 10);
        hl->setSpacing(12);

        // Иконка-заглушка (пазл)
        auto* ico = new QLabel();
        ico->setPixmap(
            ThemeManager::tintedIcon(QStringLiteral(":/icons/settings_advanced.png"))
            .pixmap(22, 22));
        ico->setFixedSize(28, 28);
        ico->setAlignment(Qt::AlignCenter);

        // Название + описание + версия
        auto* infoCol = new QVBoxLayout();
        infoCol->setSpacing(2);
        infoCol->setContentsMargins(0, 0, 0, 0);

        auto* nameLbl = new QLabel(e.plugin->name());
        nameLbl->setObjectName("settingsNavRowText");

        auto* descLbl = new QLabel(
            e.plugin->description() +
            QStringLiteral("  <span style='opacity:0.5;'>v") +
            e.plugin->version() + QStringLiteral("</span>"));
        descLbl->setObjectName("settingsHint");
        descLbl->setTextFormat(Qt::RichText);
        descLbl->setWordWrap(true);

        infoCol->addWidget(nameLbl);
        infoCol->addWidget(descLbl);

        // Кнопка настроек плагина (если есть)
        if (e.plugin->createSettingsPage() != nullptr) {
            auto* cfgBtn = new QPushButton();
            cfgBtn->setObjectName("iconBtn");
            cfgBtn->setFixedSize(30, 30);
            cfgBtn->setToolTip(tr("Настройки плагина"));
            ThemeManager::applyIcon(cfgBtn, QStringLiteral(":/icons/settings_advanced.png"),
                                    QSize(16, 16));
            cfgBtn->setCursor(Qt::PointingHandCursor);

            const QString pluginName = e.plugin->name();
            IPlugin* pluginPtr = e.plugin;
            connect(cfgBtn, &QPushButton::clicked, this, [pluginPtr, pluginName, this]() {
                QWidget* page = pluginPtr->createSettingsPage();
                if (!page) return;

                auto* dlg = new QDialog(window());
                dlg->setWindowTitle(pluginPtr->settingsPageTitle());
                dlg->setMinimumSize(480, 400);
                dlg->setAttribute(Qt::WA_DeleteOnClose);

                auto* dl = new QVBoxLayout(dlg);
                dl->setContentsMargins(0, 0, 0, 0);
                dl->setSpacing(0);

                // Хедер диалога
                auto* hdr = new QWidget();
                hdr->setObjectName("settingsCardHeader");
                hdr->setFixedHeight(52);
                auto* hdrl = new QHBoxLayout(hdr);
                hdrl->setContentsMargins(16, 0, 12, 0);

                auto* titleLbl = new QLabel(pluginPtr->settingsPageTitle());
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
            });

            hl->addWidget(ico);
            hl->addLayout(infoCol, 1);
            hl->addWidget(cfgBtn);
        } else {
            hl->addWidget(ico);
            hl->addLayout(infoCol, 1);
        }

        // Тогл включения/выключения
        auto* tog = new ToggleSwitch();
        tog->setChecked(e.enabled);
        const QString pluginId = e.plugin->id();
        connect(tog, &ToggleSwitch::toggled, this, [pluginId](bool on) {
            PluginManager::instance().setEnabled(pluginId, on);
        });
        hl->addWidget(tog);

        m_listLay->addWidget(row);
        m_listLay->addWidget(spSeparator());
    }
}

void SettingsPluginsPage::openPluginsFolder() const {
    const QString dir = PluginManager::pluginsDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
