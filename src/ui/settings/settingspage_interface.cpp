#include "settingspage_interface.h"
#include "settingshelpers.h"
#include "../wheelfilter.h"
#include "../thememanager.h"
#include "../customthememanager.h"
#include "../../core/sessionmanager.h"
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>

SettingsInterfacePage::SettingsInterfacePage(QWidget* parent) : SettingsPageBase(parent) {
    m_lay->addWidget(spFieldLabel(tr("Theme")));
    m_lay->addSpacing(4);

    m_themeCombo = new QComboBox();
    m_themeCombo->setObjectName("settingsInput");
    noScrollWheel(m_themeCombo);
    m_themeCombo->addItem("◐  " + tr("Dark"),       "dark");
    m_themeCombo->addItem("○  " + tr("Light"),      "light");
    m_themeCombo->addItem("●  " + tr("B&W"),        "bw");
    m_themeCombo->addItem("🌲  " + tr("Forest"),    "forest");
    m_themeCombo->addItem("🌃  " + tr("Cyberpunk"), "cyberpunk");
    m_themeCombo->addItem("❄  " + tr("Nordic"),     "nordic");
    m_themeCombo->addItem("🌅  " + tr("Sunset"),    "sunset");
    rebuildCustomThemeItems();

    {
        const QString cur = QString::fromStdString(SessionManager::instance().theme());
        const int idx = m_themeCombo->findData(cur.isEmpty() ? "dark" : cur);
        if (idx >= 0) m_themeCombo->setCurrentIndex(idx);
    }

    m_customRestartHint = spHint(tr("Требуется перезапуск для применения темы"));
    m_customRestartHint->setVisible(false);

    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        const QString s = m_themeCombo->currentData().toString();
        m_removeThemeBtn->setEnabled(s.startsWith("custom:"));

        if (s.startsWith("custom:")) {
            const QString folder = s.mid(7);
            if (ThemeManager::instance().loadCustomTheme(folder))
                ThemeManager::instance().applyCustomTheme();
            else
                QMessageBox::warning(this, tr("Ошибка темы"),
                    tr("Не удалось загрузить тему. Возможно, файл повреждён."));
            return;
        }

        Theme t = Theme::Dark;
        if      (s == "light")     t = Theme::Light;
        else if (s == "bw")        t = Theme::BW;
        else if (s == "forest")    t = Theme::Forest;
        else if (s == "cyberpunk") t = Theme::Cyberpunk;
        else if (s == "nordic")    t = Theme::Nordic;
        else if (s == "sunset")    t = Theme::Sunset;
        ThemeManager::instance().setTheme(t);
    });

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, m_themeCombo, [this](Theme t) {
        QString s = "dark";
        switch (t) {
            case Theme::Light:     s = "light";     break;
            case Theme::BW:        s = "bw";        break;
            case Theme::Forest:    s = "forest";    break;
            case Theme::Cyberpunk: s = "cyberpunk"; break;
            case Theme::Nordic:    s = "nordic";    break;
            case Theme::Sunset:    s = "sunset";    break;
            default: break;
        }
        const int idx = m_themeCombo->findData(s);
        if (idx >= 0 && m_themeCombo->currentIndex() != idx)
            m_themeCombo->setCurrentIndex(idx);
    });

    m_lay->addWidget(m_themeCombo);
    m_lay->addWidget(m_customRestartHint);

    m_importThemeBtn = new QPushButton(tr("Import theme..."));
    m_importThemeBtn->setObjectName("dlgCancelBtn");
    connect(m_importThemeBtn, &QPushButton::clicked, this, &SettingsInterfacePage::onImportTheme);

    m_removeThemeBtn = new QPushButton(tr("Remove theme"));
    m_removeThemeBtn->setObjectName("dlgCancelBtn");
    m_removeThemeBtn->setEnabled(false);
    connect(m_removeThemeBtn, &QPushButton::clicked, this, &SettingsInterfacePage::onRemoveTheme);

    auto* themeBtnRow = new QHBoxLayout();
    themeBtnRow->setSpacing(8);
    themeBtnRow->addWidget(m_importThemeBtn);
    themeBtnRow->addWidget(m_removeThemeBtn);
    themeBtnRow->addStretch();
    m_lay->addLayout(themeBtnRow);
    m_lay->addSpacing(12);

    m_lay->addWidget(spFieldLabel(tr("Language")));
    m_langCombo = new QComboBox();
    m_langCombo->setObjectName("settingsInput");
    noScrollWheel(m_langCombo);
    m_langCombo->addItem(tr("Russian"),     "ru");
    m_langCombo->addItem(tr("English"),     "en");
    m_langCombo->addItem(tr("Belarusian"),  "be");
    m_langCombo->addItem(tr("Ukrainian"),   "uk");
    m_langCombo->addItem(tr("German"),      "de");
    m_lay->addWidget(m_langCombo);
    m_lay->addWidget(spHint(tr("Requires restart")));
    m_lay->addSpacing(12);

    m_enterSendsCheck = new QCheckBox(tr("Enter отправляет сообщение"));
    m_enterSendsCheck->setChecked(SessionManager::instance().enterSends());
    connect(m_enterSendsCheck, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        const bool on = (state == Qt::Checked);
        SessionManager::instance().setEnterSends(on);
        emit enterSendsChanged(on);
    });
    m_lay->addWidget(m_enterSendsCheck);
    m_lay->addWidget(spHint(tr("Shift+Enter — новая строка. "
                               "Если выключено: Enter — новая строка, Ctrl+Enter — отправить.")));
    m_lay->addStretch();
}

void SettingsInterfacePage::reload() {
    auto& sm = SessionManager::instance();
    const int idx = m_langCombo->findData(QString::fromStdString(sm.language()));
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);

    rebuildCustomThemeItems();
    const QString key = sm.theme().empty() ? "dark" : QString::fromStdString(sm.theme());
    const int themeIdx = m_themeCombo->findData(key);
    if (themeIdx >= 0) m_themeCombo->setCurrentIndex(themeIdx);
    m_removeThemeBtn->setEnabled(key.startsWith("custom:"));

    if (m_enterSendsCheck)
        m_enterSendsCheck->setChecked(sm.enterSends());
}

bool SettingsInterfacePage::save() {
    SessionManager::instance().setLanguage(m_langCombo->currentData().toString().toStdString());
    return true;
}

void SettingsInterfacePage::rebuildCustomThemeItems() {
    while (m_themeCombo->count() > 7)
        m_themeCombo->removeItem(m_themeCombo->count() - 1);

    const QList<CustomThemeMeta> customs = CustomThemeManager::scan();
    if (customs.isEmpty()) return;

    m_themeCombo->insertSeparator(m_themeCombo->count());
    for (const CustomThemeMeta& meta : customs) {
        const QString label = "🎨  " + meta.displayName
                              + (meta.author.isEmpty() ? "" : " (" + meta.author + ")");
        m_themeCombo->addItem(label, "custom:" + meta.folderName);
    }
}

void SettingsInterfacePage::onImportTheme() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Импортировать тему"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        tr("Архивы тем (*.zip *.tar.gz *.tgz *.7z)"),
        nullptr,
        QFileDialog::DontUseNativeDialog);

    if (path.isEmpty()) return;
    QString error, folderName;
    if (!CustomThemeManager::importArchive(path, error, &folderName)) {
        QMessageBox::critical(this, tr("Ошибка импорта"), error);
        return;
    }
    rebuildCustomThemeItems();
    const int idx = m_themeCombo->findData("custom:" + folderName);
    if (idx >= 0)
        m_themeCombo->setCurrentIndex(idx);
}

void SettingsInterfacePage::onRemoveTheme() {
    const QString s = m_themeCombo->currentData().toString();
    if (!s.startsWith("custom:")) return;
    const QString folder      = s.mid(7);
    const QString displayName = ThemeManager::instance().customThemeDisplayName();
    const int ret = QMessageBox::question(this,
        tr("Удалить тему"), tr("Удалить тему \"%1\"?").arg(displayName),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    if (ThemeManager::instance().currentTheme() == Theme::Custom)
        m_themeCombo->setCurrentIndex(0);
    if (!CustomThemeManager::removeTheme(folder)) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось удалить тему"));
        return;
    }
    rebuildCustomThemeItems();
}
