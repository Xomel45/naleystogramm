#include "thememanager.h"
#include "../core/sessionmanager.h"
#include <QApplication>
#include <QPalette>

// ── Singleton ─────────────────────────────────────────────────────────────

ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager() {
    // Читаем тему из session.json
    const QString saved = SessionManager::instance().theme();
    if      (saved == "light") m_theme = Theme::Light;
    else if (saved == "bw")    m_theme = Theme::BW;
    else                       m_theme = Theme::Dark;
    applyPalette();
}

void ThemeManager::setTheme(Theme t) {
    if (m_theme == t) return;
    m_theme = t;
    // Сохраняем в session.json
    QString name;
    switch (t) {
        case Theme::Dark:  name = "dark";  break;
        case Theme::Light: name = "light"; break;
        case Theme::BW:    name = "bw";    break;
    }
    SessionManager::instance().setTheme(name);
    applyPalette();
    qApp->setStyleSheet(stylesheet());
    emit themeChanged(t);
}

QString ThemeManager::currentThemeName() const {
    // C++20: using enum — не нужно писать Theme:: каждый раз
    using enum Theme;
    switch (m_theme) {
        case Dark:  return "Тёмная";
        case Light: return "Светлая";
        case BW:    return "Ч/Б";
    }
    return {};
}

void ThemeManager::applyPalette() {
    using enum Theme;
    switch (m_theme) {
        case Dark:  m_palette = darkPalette();  break;
        case Light: m_palette = lightPalette(); break;
        case BW:    m_palette = bwPalette();    break;
    }
}

// ═════════════════════════════════════════════════════════════════════════
// ТЁМНАЯ ТЕМА — глубокий космос, акценты индиго/фиолетовый
// ═════════════════════════════════════════════════════════════════════════

ThemePalette ThemeManager::darkPalette() {
    ThemePalette p;
    p.bg           = "#080810";
    p.bgSurface    = "#0e0e1c";
    p.bgElevated   = "#13132a";
    p.bgInput      = "#18182e";
    p.bgBubbleOut  = "#2a1f5e";
    p.bgBubbleIn   = "#161628";

    p.border       = "#1e1e3a";
    p.borderFocus  = "#6c5ce7";

    p.textPrimary   = "#ece9ff";
    p.textSecondary = "#b8b4d8";
    p.textMuted     = "#5a5880";
    p.textOnAccent  = "#ffffff";

    p.accent        = "#6c5ce7";
    p.accentHover   = "#8677ff";
    p.accentPressed = "#5449c4";

    p.online  = "#00cba9";
    p.offline = "#3a3a5c";
    p.danger  = "#ff4d6d";
    p.success = "#00cba9";
    p.bannerBg       = "#1e1b3a";
    p.bannerBorder   = "#6c5ce7";
    p.bannerText     = "#e2e2f0";
    p.bannerBtnHover = "#8075e5";
    return p;
}

// ═════════════════════════════════════════════════════════════════════════
// СВЕТЛАЯ ТЕМА — чистый белый, акценты яркий коралл/оранжевый
// ═════════════════════════════════════════════════════════════════════════

ThemePalette ThemeManager::lightPalette() {
    ThemePalette p;
    p.bg           = "#f7f5ff";
    p.bgSurface    = "#ffffff";
    p.bgElevated   = "#f0eeff";
    p.bgInput      = "#ffffff";
    p.bgBubbleOut  = "#ff6b35";
    p.bgBubbleIn   = "#ffffff";

    p.border       = "#e2deff";
    p.borderFocus  = "#ff6b35";

    p.textPrimary   = "#1a1035";
    p.textSecondary = "#4a3f6b";
    p.textMuted     = "#9b92b8";
    p.textOnAccent  = "#ffffff";

    p.accent        = "#ff6b35";
    p.accentHover   = "#ff8555";
    p.accentPressed = "#e5521c";

    p.online  = "#00b894";
    p.offline = "#c5bfe8";
    p.danger  = "#d63031";
    p.success = "#00b894";
    p.bannerBg       = "#fff3e0";
    p.bannerBorder   = "#ff6b35";
    p.bannerText     = "#2d1a0e";
    p.bannerBtnHover = "#ff8c5a";
    return p;
}

// ═════════════════════════════════════════════════════════════════════════
// Ч/Б ТЕМА — только чёрный, белый и серые
// ═════════════════════════════════════════════════════════════════════════

ThemePalette ThemeManager::bwPalette() {
    ThemePalette p;
    p.bg           = "#0a0a0a";
    p.bgSurface    = "#141414";
    p.bgElevated   = "#1c1c1c";
    p.bgInput      = "#222222";
    p.bgBubbleOut  = "#2e2e2e";
    p.bgBubbleIn   = "#1a1a1a";

    p.border       = "#2a2a2a";
    p.borderFocus  = "#888888";

    p.textPrimary   = "#f0f0f0";
    p.textSecondary = "#aaaaaa";
    p.textMuted     = "#555555";
    p.textOnAccent  = "#000000";

    p.accent        = "#e0e0e0";
    p.accentHover   = "#ffffff";
    p.accentPressed = "#bbbbbb";

    p.online  = "#cccccc";
    p.offline = "#444444";
    p.danger  = "#888888";
    p.success = "#cccccc";
    p.bannerBg       = "#1a1a1a";
    p.bannerBorder   = "#888888";
    p.bannerText     = "#f0f0f0";
    p.bannerBtnHover = "#aaaaaa";
    return p;
}

// ═════════════════════════════════════════════════════════════════════════
// ПОЛНЫЙ QSS — один на все темы, через переменные палитры
// ═════════════════════════════════════════════════════════════════════════

QString ThemeManager::stylesheet() const {
    const auto& p = m_palette;

    return QString(R"(

/* ── Базовые виджеты ──────────────────────────────────────────────────── */

QWidget {
    background: %1;
    color: %11;
    font-family: "SF Pro Display", "Segoe UI Variable", "Cantarell", sans-serif;
    font-size: 13px;
    outline: none;
}

QMainWindow {
    background: %1;
}

/* ── Сплиттер ─────────────────────────────────────────────────────────── */

QSplitter::handle {
    background: %5;
    width: 1px;
}

/* ── Статус бар ────────────────────────────────────────────────────────── */

QStatusBar {
    background: %3;
    color: %13;
    font-size: 11px;
    border-top: 1px solid %5;
    padding: 0 12px;
}
QStatusBar::item { border: none; }

/* ══════════════════════════════════════════════════════════════════════
   ЛЕВАЯ ПАНЕЛЬ
══════════════════════════════════════════════════════════════════════ */

QWidget#leftPanel {
    background: %2;
    border-right: 1px solid %5;
}

/* ── Хедер с именем ─────────────────────────────────────────────────── */

QWidget#headerBar {
    background: %3;
    border-bottom: 1px solid %5;
}

QLabel#myNameLabel {
    color: %11;
    font-size: 15px;
    font-weight: 700;
    letter-spacing: -0.3px;
    padding: 0 4px;
}

/* ── Поиск ──────────────────────────────────────────────────────────── */

QLineEdit#searchInput {
    background: %4;
    border: 1px solid %5;
    border-radius: 20px;
    padding: 7px 16px;
    color: %11;
    font-size: 13px;
    selection-background-color: %15;
}
QLineEdit#searchInput:focus {
    border-color: %6;
}
QLineEdit#searchInput::placeholder {
    color: %13;
}

/* ── Список контактов ───────────────────────────────────────────────── */

QListWidget {
    background: transparent;
    border: none;
    padding: 4px 6px;
    outline: none;
}
QListWidget::item {
    background: transparent;
    border-radius: 12px;
    padding: 10px 8px;
    color: %12;
    margin: 1px 0;
    border: 1px solid transparent;
}
QListWidget::item:hover {
    background: %3;
    border-color: %5;
}
QListWidget::item:selected {
    background: %3;
    border: 1px solid %6;
    color: %11;
}

/* ── Кнопка добавить контакт ────────────────────────────────────────── */

QPushButton#addContactBtn {
    background: %15;
    color: %16;
    border: none;
    border-radius: 14px;
    padding: 11px 16px;
    margin: 6px 10px 14px;
    font-weight: 700;
    font-size: 13px;
    letter-spacing: 0.2px;
    text-align: center;
}
QPushButton#addContactBtn:hover {
    background: %17;
}
QPushButton#addContactBtn:pressed {
    background: %18;
}

/* ── Иконочные кнопки (🔑, 📎, etc) ─────────────────────────────────── */

QPushButton#iconBtn {
    background: transparent;
    border: none;
    border-radius: 8px;
    color: %13;
    font-size: 15px;
    padding: 4px;
}
QPushButton#iconBtn:hover {
    background: %3;
    color: %11;
}
QPushButton#iconBtn:pressed {
    background: %5;
}

/* ══════════════════════════════════════════════════════════════════════
   ПРАВАЯ ПАНЕЛЬ — ЧАТ
══════════════════════════════════════════════════════════════════════ */

/* ── Хедер чата ─────────────────────────────────────────────────────── */

QWidget#chatHeader {
    background: %3;
    border-bottom: 1px solid %5;
}

QLabel#chatPeerName {
    font-size: 15px;
    font-weight: 700;
    color: %11;
    letter-spacing: -0.2px;
}

QLabel#chatPeerStatus {
    font-size: 11px;
    color: %13;
    letter-spacing: 0.3px;
}

QLabel#peerAvatar {
    background: %5;
    border-radius: 19px;
    color: %13;
    font-size: 16px;
    border: 2px solid %5;
}

/* ── Область сообщений ──────────────────────────────────────────────── */

QScrollArea {
    background: %1;
    border: none;
}
QScrollArea > QWidget > QWidget {
    background: %1;
}

QScrollBar:vertical {
    background: transparent;
    width: 4px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: %5;
    border-radius: 2px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover {
    background: %13;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
}

/* ── Поле ввода ─────────────────────────────────────────────────────── */

QWidget#inputBar {
    background: %3;
    border-top: 1px solid %5;
}

QTextEdit#msgInput {
    background: %4;
    border: 1px solid %5;
    border-radius: 16px;
    padding: 10px 14px;
    color: %11;
    font-size: 14px;
    selection-background-color: %15;
    line-height: 1.4;
}
QTextEdit#msgInput:focus {
    border-color: %6;
}

/* ── Кнопка отправки ────────────────────────────────────────────────── */

QPushButton#sendBtn {
    background: %15;
    color: %16;
    border: none;
    border-radius: 22px;
    font-size: 15px;
    font-weight: 700;
}
QPushButton#sendBtn:hover {
    background: %17;
}
QPushButton#sendBtn:pressed {
    background: %18;
}
QPushButton#sendBtn:disabled {
    background: %5;
    color: %13;
}

/* ══════════════════════════════════════════════════════════════════════
   ДИАЛОГИ
══════════════════════════════════════════════════════════════════════ */

QDialog {
    background: %2;
}

QLabel#dlgTitle {
    color: %11;
    font-size: 17px;
    font-weight: 700;
    letter-spacing: -0.3px;
}
QLabel#dlgSubtitle {
    color: %13;
    font-size: 12px;
    line-height: 1.5;
}
QLabel#dlgError {
    color: %19;
    font-size: 12px;
}

QTextEdit#dlgInput {
    background: %4;
    border: 1px solid %5;
    border-radius: 10px;
    padding: 10px 12px;
    color: %11;
    font-family: "SF Mono", "Cascadia Code", "Fira Code", monospace;
    font-size: 12px;
}
QTextEdit#dlgInput:focus {
    border-color: %6;
}

QPushButton#dlgOkBtn {
    background: %15;
    color: %16;
    border: none;
    border-radius: 10px;
    padding: 10px 20px;
    font-weight: 700;
    font-size: 13px;
}
QPushButton#dlgOkBtn:hover    { background: %17; }
QPushButton#dlgOkBtn:pressed  { background: %18; }

QPushButton#dlgCancelBtn {
    background: %4;
    color: %12;
    border: 1px solid %5;
    border-radius: 10px;
    padding: 10px 20px;
    font-weight: 600;
    font-size: 13px;
}
QPushButton#dlgCancelBtn:hover { color: %11; border-color: %6; }

QPushButton#dlgAcceptBtn {
    background: transparent;
    color: %20;
    border: 1.5px solid %20;
    border-radius: 10px;
    padding: 10px 20px;
    font-weight: 700;
    font-size: 13px;
}
QPushButton#dlgAcceptBtn:hover {
    background: rgba(0, 203, 169, 0.12);
}

QPushButton#dlgRejectBtn {
    background: transparent;
    color: %19;
    border: 1.5px solid %19;
    border-radius: 10px;
    padding: 10px 20px;
    font-weight: 700;
    font-size: 13px;
}
QPushButton#dlgRejectBtn:hover {
    background: rgba(255, 77, 109, 0.1);
}

/* ── QMessageBox ────────────────────────────────────────────────────── */

QMessageBox {
    background: %2;
}
QMessageBox QLabel {
    color: %11;
}
QMessageBox QPushButton {
    background: %4;
    color: %11;
    border: 1px solid %5;
    border-radius: 8px;
    padding: 8px 16px;
    min-width: 80px;
}
QMessageBox QPushButton:hover {
    border-color: %6;
}
QMessageBox QPushButton:default {
    background: %15;
    color: %16;
    border: none;
}

/* ── QInputDialog ─────────────────────────────────────────────────────── */

QInputDialog {
    background: %2;
}
QInputDialog QLabel {
    color: %11;
}
QInputDialog QLineEdit {
    background: %4;
    border: 1px solid %5;
    border-radius: 8px;
    padding: 8px 12px;
    color: %11;
}
QInputDialog QLineEdit:focus { border-color: %6; }
QInputDialog QPushButton {
    background: %4;
    color: %11;
    border: 1px solid %5;
    border-radius: 8px;
    padding: 8px 16px;
}
QInputDialog QPushButton:hover { border-color: %6; }
QInputDialog QPushButton:default {
    background: %15;
    color: %16;
    border: none;
}

/* ── Сплиттер ────────────────────────────────────────────────────────── */

QSplitter#mainSplitter::handle {
    background: %5;
    width: 4px;
}
QSplitter#mainSplitter::handle:hover {
    background: %6;
}
QSplitter#mainSplitter::handle:pressed {
    background: %15;
}

/* ── UpdateBanner ────────────────────────────────────────────────────── */

QWidget#updateBanner {
    background: %21;
    border-bottom: 1px solid %22;
}
QLabel#updateBannerText {
    color: %23;
    font-size: 12px;
    font-weight: 600;
}
QPushButton#updateBannerBtn {
    background: %22;
    border: none;
    border-radius: 5px;
    color: %23;
    font-size: 11px;
    font-weight: 700;
    padding: 2px 10px;
}
QPushButton#updateBannerBtn:hover {
    background: %24;
}

/* ── SettingsDialog ──────────────────────────────────────────────────── */

QListWidget#settingsNav {
    background: %2;
    border: none;
    padding: 8px 0;
    outline: none;
}
QListWidget#settingsNav::item {
    color: %12;
    border-radius: 0;
    padding: 0;
    font-size: 13px;
    font-weight: 500;
    border-left: 2px solid transparent;
}
QListWidget#settingsNav::item:hover {
    background: %3;
    color: %11;
}
QListWidget#settingsNav::item:selected {
    background: %3;
    color: %11;
    border-left: 2px solid %15;
}

QFrame#settingsDivider {
    color: %5;
    max-width: 1px;
}

QWidget#settingsStack {
    background: %1;
}

QLabel#settingsPageTitle {
    font-size: 18px;
    font-weight: 700;
    color: %11;
    letter-spacing: -0.3px;
    padding-bottom: 4px;
}

QLabel#settingsFieldLabel {
    font-size: 12px;
    font-weight: 600;
    color: %12;
    letter-spacing: 0.4px;
    text-transform: uppercase;
    margin-top: 4px;
}

QLabel#settingsHint {
    font-size: 11px;
    color: %13;
    line-height: 1.5;
}

QLineEdit#settingsInput,
QSpinBox#settingsInput,
QComboBox#settingsInput {
    background: %4;
    border: 1px solid %5;
    border-radius: 8px;
    padding: 8px 12px;
    color: %11;
    font-size: 13px;
    selection-background-color: %15;
}
QLineEdit#settingsInput:focus,
QSpinBox#settingsInput:focus,
QComboBox#settingsInput:focus {
    border-color: %6;
}
QLineEdit#settingsInputMono {
    background: %3;
    border: 1px solid %5;
    border-radius: 8px;
    padding: 8px 12px;
    color: %13;
    font-family: "SF Mono", "Cascadia Code", "Fira Code", monospace;
    font-size: 11px;
}

QComboBox#settingsInput::drop-down {
    border: none;
    padding-right: 8px;
}
QComboBox#settingsInput QAbstractItemView {
    background: %3;
    border: 1px solid %5;
    color: %11;
    selection-background-color: %15;
    selection-color: %16;
}

QSpinBox#settingsInput::up-button,
QSpinBox#settingsInput::down-button {
    background: %3;
    border: none;
    width: 20px;
}

QWidget#settingsInfoBox {
    background: %3;
    border: 1px solid %5;
    border-radius: 8px;
}

QLabel#settingsOk  { color: %20; font-size: 16px; }
QLabel#settingsWarn { color: %19; font-size: 16px; }

QPushButton#demoToggleBtn {
    background: %3;
    border: 1.5px solid %5;
    border-radius: 8px;
    color: %12;
    font-size: 12px;
    font-weight: 600;
    padding: 7px 16px;
}
QPushButton#demoToggleBtn:hover {
    border-color: %6;
    color: %11;
}
QPushButton#demoToggleBtn:checked {
    background: %18;
    border-color: %19;
    color: #ffffff;
}

QPushButton#settingsThemeRow {
    background: %3;
    border: 1.5px solid %5;
    border-radius: 8px;
    color: %12;
    font-size: 13px;
    font-weight: 600;
    padding: 6px 12px;
    text-align: left;
}
QPushButton#settingsThemeRow:hover {
    border-color: %6;
    color: %11;
}
QPushButton#settingsThemeRow:checked {
    background: %4;
    border-color: %15;
    border-width: 2px;
    color: %11;
}

QFrame#settingsSeparator {
    color: %5;
    max-height: 1px;
}

QScrollArea#settingsScroll {
    background: %1;
    border: none;
}
QWidget#settingsContent {
    background: %1;
}
QWidget#settingsPanel {
    background: %1;
}

/* ── Карточки тем в настройках */
QPushButton#settingsThemeCard {
    background: %3;
    border: 1.5px solid %5;
    border-radius: 10px;
    color: %12;
    font-size: 12px;
    font-weight: 600;
    padding: 8px;
    text-align: center;
}
QPushButton#settingsThemeCard:hover {
    border-color: %6;
    color: %11;
}
QPushButton#settingsThemeCard:checked {
    background: %4;
    border-color: %15;
    border-width: 2px;
    color: %11;
}

QLabel#settingsAvatar {
    background: %5;
    border-radius: 32px;
    color: %11;
    font-size: 26px;
    font-weight: 700;
    border: 2px solid %6;
}

QWidget#settingsVersionFooter {
    background: %2;
    border-top: 1px solid %5;
}
QLabel#settingsVersionLabel {
    color: %13;
    font-size: 11px;
    font-weight: 500;
    letter-spacing: 0.5px;
}

QWidget#settingsBtnBar {
    background: %2;
    border-top: 1px solid %5;
}

/* ── Футер выбора темы ──────────────────────────────────────────────── */

QWidget#themeFooter {
    background: %2;
    border-top: 1px solid %5;
}

QPushButton#themePillBtn {
    background: transparent;
    border: 1px solid %5;
    border-radius: 10px;
    color: %13;
    font-size: 11px;
    font-weight: 600;
    padding: 5px 4px;
    letter-spacing: 0.3px;
}
QPushButton#themePillBtn:hover {
    border-color: %6;
    color: %11;
    background: %3;
}
QPushButton#themePillBtn:checked {
    background: %15;
    border-color: %15;
    color: %16;
}
QPushButton#themePillBtn:checked:hover {
    background: %17;
    border-color: %17;
}

/* ── Переключатель тем (старый, для совместимости) ───────────────────── */

QPushButton#themeBtn {
    background: %4;
    border: 1px solid %5;
    border-radius: 16px;
    color: %12;
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 0.5px;
    padding: 4px 12px;
    text-transform: uppercase;
}
QPushButton#themeBtn:hover {
    border-color: %6;
    color: %11;
}
QPushButton#themeBtn:pressed {
    background: %3;
}

/* ── ToolTip ────────────────────────────────────────────────────────── */

QToolTip {
    background: %3;
    color: %11;
    border: 1px solid %5;
    border-radius: 6px;
    padding: 4px 8px;
    font-size: 12px;
}

    )")
    /* Фоны */
    .arg(p.bg)           // %1
    .arg(p.bgSurface)    // %2
    .arg(p.bgElevated)   // %3
    .arg(p.bgInput)      // %4
    .arg(p.border)       // %5
    .arg(p.borderFocus)  // %6
    /* пустышки %7 %8 %9 %10 — для резерва */
    .arg("")             // %7
    .arg("")             // %8
    .arg("")             // %9
    .arg("")             // %10
    /* Текст */
    .arg(p.textPrimary)    // %11
    .arg(p.textSecondary)  // %12
    .arg(p.textMuted)      // %13
    .arg("")               // %14
    /* Акцент */
    .arg(p.accent)         // %15
    .arg(p.textOnAccent)   // %16
    .arg(p.accentHover)    // %17
    .arg(p.accentPressed)  // %18
    /* Статусы */
    .arg(p.danger)         // %19
    .arg(p.success)        // %20
    .arg(p.bannerBg)       // %21
    .arg(p.bannerBorder)   // %22
    .arg(p.bannerText)     // %23
    .arg(p.bannerBtnHover);// %24
}
