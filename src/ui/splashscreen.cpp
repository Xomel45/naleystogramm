#include "splashscreen.h"
#include "thememanager.h"
#include "../core/systeminfo.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QApplication>
#include <QScreen>
#include <QTime>
#include <QRandomGenerator>
#include <QGraphicsOpacityEffect>

// ── SplashScreen ──────────────────────────────────────────────────────────────

SplashScreen::SplashScreen(QWidget* parent)
    : QDialog(parent,
              Qt::Dialog |
              Qt::FramelessWindowHint |
              Qt::WindowStaysOnTopHint)
{
    setFixedSize(480, 300);
    setObjectName("splashScreen");

    // Legacy-тема: оригинальный хардкод эпохи 0.1.0–0.5.0.
    // Все остальные темы — берём цвета из активной палитры ThemeManager.
    const bool isLegacy = ThemeManager::instance().currentTheme() == Theme::Legacy;

    QString bgColor, borderColor, logoColor, mutedColor, statusColor, pbBgColor, pbChunkColor;
    if (isLegacy) {
        bgColor      = "#0e0e1a";
        borderColor  = "#2a2a4a";
        logoColor    = "#7c6aff";
        mutedColor   = "#4a4a7a";
        statusColor  = "#a0a0c0";
        pbBgColor    = "#1e1e36";
        pbChunkColor = "#7c6aff";
    } else {
        const ThemePalette& p = ThemeManager::instance().palette();
        bgColor      = p.bg;
        borderColor  = p.border;
        logoColor    = p.accent;
        mutedColor   = p.textMuted;
        statusColor  = p.textSecondary;
        pbBgColor    = p.bgElevated;
        pbChunkColor = p.accent;
    }

    setStyleSheet(QString(R"(
        QDialog#splashScreen {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 14px;
        }
        QLabel {
            background: transparent;
        }
        QLabel#splashLogo {
            color: %3;
            font-size: 34px;
            font-weight: bold;
            letter-spacing: 2px;
        }
        QLabel#splashVersion {
            color: %4;
            font-size: 12px;
            letter-spacing: 1px;
        }
        QLabel#splashStatus {
            color: %5;
            font-size: 12px;
        }
        QLabel#splashPhrase {
            color: %4;
            font-size: 11px;
            font-style: italic;
        }
        QProgressBar {
            background-color: %6;
            border: none;
            border-radius: 3px;
        }
        QProgressBar::chunk {
            background-color: %7;
            border-radius: 3px;
        }
    )")
    .arg(bgColor)       // %1
    .arg(borderColor)   // %2
    .arg(logoColor)     // %3
    .arg(mutedColor)    // %4
    .arg(statusColor)   // %5
    .arg(pbBgColor)     // %6
    .arg(pbChunkColor)  // %7
    );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(48, 40, 48, 32);
    root->setSpacing(0);

    // ── Логотип ──────────────────────────────────────────────────────────────
    m_logoLabel = new QLabel("Naleystogramm");
    m_logoLabel->setObjectName("splashLogo");
    m_logoLabel->setAlignment(Qt::AlignCenter);

    m_versionLabel = new QLabel("v0.7.4  \"ыЪы\"");
    m_versionLabel->setObjectName("splashVersion");
    m_versionLabel->setAlignment(Qt::AlignCenter);

    root->addStretch(2);
    root->addWidget(m_logoLabel);
    root->addSpacing(6);
    root->addWidget(m_versionLabel);
    root->addStretch(2);

    // ── Забавная фраза ───────────────────────────────────────────────────────
    m_phraseLabel = new QLabel(getRandomPhrase());
    m_phraseLabel->setObjectName("splashPhrase");
    m_phraseLabel->setAlignment(Qt::AlignCenter);
    m_phraseLabel->setWordWrap(true);
    root->addWidget(m_phraseLabel);
    root->addSpacing(10);

    // ── Строка статуса ───────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("Инициализация..."));
    m_statusLabel->setObjectName("splashStatus");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_statusLabel);
    root->addSpacing(8);

    // ── Прогресс-бар ─────────────────────────────────────────────────────────
    m_progress = new QProgressBar();
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(10);
    root->addWidget(m_progress);
    root->addSpacing(8);

    // ── Подпись авторов — появляется постепенно по мере загрузки ─────────────
    m_creditsLabel = new QLabel("Made by Xomelz & Claude");
    m_creditsLabel->setObjectName("splashVersion");
    m_creditsLabel->setAlignment(Qt::AlignCenter);
    m_creditsOpacity = new QGraphicsOpacityEffect(m_creditsLabel);
    m_creditsOpacity->setOpacity(0.0);
    m_creditsLabel->setGraphicsEffect(m_creditsOpacity);
    root->addWidget(m_creditsLabel);

    // Центрируем окно на экране
    if (const QScreen* screen = QApplication::primaryScreen()) {
        const QRect sg = screen->geometry();
        move(sg.center() - rect().center());
    }
}

// ── updateStatus ──────────────────────────────────────────────────────────────

void SplashScreen::updateStatus(int progress, const QString& status) {
    m_progress->setValue(progress);
    m_statusLabel->setText(status);

    // Подпись авторов: плавно проявляется по мере заполнения прогресс-бара
    if (m_creditsOpacity)
        m_creditsOpacity->setOpacity(qBound(0.0, progress / 100.0, 1.0));

    // Меняем фразу в контрольных точках чтобы разнообразить экран
    if (progress == 0 || progress % 25 == 0 || progress >= 100)
        m_phraseLabel->setText(getRandomPhrase());

    // Принудительно обрабатываем события — иначе UI не перерисуется
    QApplication::processEvents();
}

// ── getRandomPhrase ───────────────────────────────────────────────────────────
// Возвращает случайную фразу на основе вероятностей и времени суток.
// Специальные фразы проверяются первыми (по убыванию приоритета).

QString SplashScreen::getRandomPhrase() const {
    const int h    = QTime::currentTime().hour();
    const int roll = static_cast<int>(QRandomGenerator::global()->bounded(100u));

    // 1. "Алина секси?" — 1% шанс, только с 18:00 до 05:00
    if (roll < 1 && (h >= 18 || h < 5))
        return "Алина секси?";

    // 2. "А я знаю что ты дрочишь!" — 5% шанс
    //    Только в: 00:00–04:00, 14:00–16:00, 20:00–22:00
    if (roll < 6 && (h < 4 || (h >= 14 && h < 16) || (h >= 20 && h < 22)))
        return "А я знаю что ты дрочишь!";

    // 3. "Я люблю {CPU}" — 5% шанс (используем модель CPU из SystemInfo)
    if (roll < 11) {
        const QString cpu = SystemInfo::instance().cpuModel();
        return QString("Я люблю %1").arg(cpu.isEmpty() ? "этот компьютер" : cpu);
    }

    // 4. "Кто-то это действительно читает?" — 10% шанс
    if (roll < 21)
        return "Кто-то это действительно читает?";

    // 5. "Генерация ландыша" — 5% шанс
    if (roll < 26)
        return "Генерация ландыша";

    // Обычные фразы — выбираем случайную из набора (69% суммарно)
    const QStringList normal = {
        "Загрузка секретных данных...",
        "Проверка связи с бункером...",
        "Шифрование всего подряд...",
        "Не выключайте компьютер, пожалуйста...",
    };
    return normal[static_cast<int>(
        QRandomGenerator::global()->bounded(static_cast<quint32>(normal.size())))];
}
