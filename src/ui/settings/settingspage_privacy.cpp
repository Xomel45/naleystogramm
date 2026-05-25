#include "settingspage_privacy.h"
#include "settingshelpers.h"
#include "../wheelfilter.h"
#include "../../core/sessionmanager.h"
#include "../../core/types.h"
#include <QComboBox>

SettingsPrivacyPage::SettingsPrivacyPage(QWidget* parent) : SettingsPageBase(parent) {
    auto makeRow = [&](const QString& label, QComboBox*& combo) {
        m_lay->addWidget(spFieldLabel(label));
        m_lay->addSpacing(4);
        combo = new QComboBox();
        combo->setObjectName("settingsInput");
        combo->addItem(tr("Все"),             static_cast<int>(PrivacyLevel::Everyone));
        combo->addItem(tr("Только контакты"), static_cast<int>(PrivacyLevel::ContactsOnly));
        combo->addItem(tr("Никто"),           static_cast<int>(PrivacyLevel::Nobody));
        noScrollWheel(combo);
        m_lay->addWidget(combo);
        m_lay->addSpacing(8);
    };

    makeRow(tr("Кто может писать"),           m_messages);
    makeRow(tr("Кто может отправлять файлы"), m_files);
    makeRow(tr("Кто может звонить"),          m_calls);
    makeRow(tr("Кто может слать голосовые"),  m_voice);
    makeRow(tr("Кто видит аватар"),           m_avatar);
    makeRow(tr("Кто может запросить шелл"),   m_shell);

    m_lay->addWidget(spHint(
        tr("«Только контакты» — разрешает действие только от людей из вашего списка контактов.\n"
           "Изменения применяются после нажатия «Сохранить».")));
    m_lay->addStretch();
}

void SettingsPrivacyPage::reload() {
    auto& sm = SessionManager::instance();
    auto sync = [](QComboBox* c, PrivacyLevel lv) {
        const int val = static_cast<int>(lv);
        for (int i = 0; i < c->count(); ++i)
            if (c->itemData(i).toInt() == val) { c->setCurrentIndex(i); return; }
    };
    sync(m_messages, sm.privacyMessages());
    sync(m_files,    sm.privacyFiles());
    sync(m_calls,    sm.privacyCalls());
    sync(m_voice,    sm.privacyVoice());
    sync(m_avatar,   sm.privacyAvatar());
    sync(m_shell,    sm.privacyShell());
}

bool SettingsPrivacyPage::save() {
    auto& sm = SessionManager::instance();
    sm.setPrivacyMessages(static_cast<PrivacyLevel>(m_messages->currentData().toInt()));
    sm.setPrivacyFiles   (static_cast<PrivacyLevel>(m_files->currentData().toInt()));
    sm.setPrivacyCalls   (static_cast<PrivacyLevel>(m_calls->currentData().toInt()));
    sm.setPrivacyVoice   (static_cast<PrivacyLevel>(m_voice->currentData().toInt()));
    sm.setPrivacyAvatar  (static_cast<PrivacyLevel>(m_avatar->currentData().toInt()));
    sm.setPrivacyShell   (static_cast<PrivacyLevel>(m_shell->currentData().toInt()));
    return true;
}
