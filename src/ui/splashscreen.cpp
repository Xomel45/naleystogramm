#include "splashscreen.h"
#include "../core/systeminfo.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QApplication>
#include <QScreen>
#include <QTime>
#include <QRandomGenerator>

// ── SplashScreen ──────────────────────────────────────────────────────────────

SplashScreen::SplashScreen(QWidget* parent)
    : QDialog(parent,
              Qt::Dialog |
              Qt::FramelessWindowHint |
              Qt::WindowStaysOnTopHint)
{
    setFixedSize(480, 300);
    setObjectName("splashScreen");

    // Тёмный стиль — не зависит от темы приложения, всегда тёмный
    setStyleSheet(R"(
        QDialog#splashScreen {
            background-color: #0e0e1a;
            border: 1px solid #2a2a4a;
            border-radius: 14px;
        }
        QLabel#splashLogo {
            color: #7c6aff;
            font-size: 34px;
            font-weight: bold;
            letter-spacing: 2px;
        }
        QLabel#splashVersion {
            color: #4a4a7a;
            font-size: 12px;
            letter-spacing: 1px;
        }
        QLabel#splashStatus {
            color: #a0a0c0;
            font-size: 12px;
        }
        QLabel#splashPhrase {
            color: #4a4a7a;
            font-size: 11px;
            font-style: italic;
        }
        QProgressBar {
            background-color: #1e1e36;
            border: none;
            border-radius: 3px;
        }
        QProgressBar::chunk {
            background-color: #7c6aff;
            border-radius: 3px;
        }
    )");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(48, 40, 48, 32);
    root->setSpacing(0);

    // ── Логотип ──────────────────────────────────────────────────────────────
    m_logoLabel = new QLabel("Naleystogramm");
    m_logoLabel->setObjectName("splashLogo");
    m_logoLabel->setAlignment(Qt::AlignCenter);

    m_versionLabel = new QLabel("v0.5.3  \"Консервная банка\"");
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
    root->addSpacing(10);

    // ── Прогресс-бар ─────────────────────────────────────────────────────────
    m_progress = new QProgressBar();
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(5);
    root->addWidget(m_progress);

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
