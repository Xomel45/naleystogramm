#include <QApplication>
#include <QFont>
#include <QNetworkProxy>
#include <QTranslator>
#include <QLibraryInfo>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QEventLoop>
#include <QThread>
#include <QThreadPool>
#include <QTextStream>
#include "ui/mainwindow.h"
#include "ui/splashscreen.h"
#include "core/sessionmanager.h"
#include "core/systeminfo.h"
#include "core/logger.h"

// Путь к переводам (передаётся из CMake)
#ifndef TRANSLATIONS_DIR
    #define TRANSLATIONS_DIR "translations"
#endif

// Загрузка перевода по коду языка (ru, en)
static QTranslator* loadTranslation(QApplication& app, const QString& lang) {
    auto* translator = new QTranslator(&app);

    // Пути поиска переводов (в порядке приоритета)
    const QStringList searchPaths = {
        TRANSLATIONS_DIR,                                    // Из CMake
        QCoreApplication::applicationDirPath() + "/translations",  // Рядом с exe
        ":/translations",                                    // Встроенные ресурсы
        "/usr/share/naleystogramm/translations"              // Системный путь Linux
    };

    const QString fileName = QString("naleystogramm_%1").arg(lang);

    for (const QString& path : searchPaths) {
        if (translator->load(fileName, path)) {
            qDebug("[i18n] Loaded translation: %s/%s.qm",
                   qPrintable(path), qPrintable(fileName));
            app.installTranslator(translator);
            return translator;
        }
    }

    qWarning("[i18n] Translation not found: %s (searched %lld paths)",
             qPrintable(fileName), static_cast<long long>(searchPaths.size()));
    delete translator;
    return nullptr;
}

// Небольшая задержка для визуализации шагов — запускает QEventLoop
static void splashDelay(int ms) {
    QTimer t;
    t.setSingleShot(true);
    QEventLoop loop;
    QObject::connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
    t.start(ms);
    loop.exec();
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("naleystogramm");
    app.setApplicationVersion("0.6.0");
    app.setOrganizationName("naleystogramm");

    // Прямое подключение — системный прокси не используется
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    // ── Аварийный сброс темы ──────────────────────────────────────────────
    // Запуск с --theme-reload сбрасывает тему до Dark без открытия UI.
    // Используется если кастомная тема привела к нечитаемому интерфейсу.
    if (app.arguments().contains("--theme-reload")) {
        SessionManager::instance().load();
        SessionManager::instance().setTheme("dark");
        QTextStream(stdout) << "Тема сброшена до тёмной (Dark).\n"
                               "Запустите приложение без --theme-reload.\n";
        return 0;
    }

    // ── Шрифт приложения ──────────────────────────────────────────────────
    QFont font;
    font.setFamily("Sans Serif");
    font.setPointSize(10);
    font.setStyleHint(QFont::SansSerif);
    app.setFont(font);

    // ── Экран загрузки ────────────────────────────────────────────────────
    // Создаём и показываем сразу — до любой тяжёлой работы
    auto* splash = new SplashScreen();
    splash->show();
    QApplication::processEvents();

    // ── Шаг 1: Загрузка пользовательской сессии ──────────────────────────
    splash->updateStatus(10, QObject::tr("Загрузка настроек..."));
    splashDelay(80);
    SessionManager::instance().load();

    // Создаём все рабочие директории сразу после загрузки сессии.
    // Это критично: Logger, Storage и KeyProtector ожидают что папки уже есть.
    SessionManager::ensureDirectories();

    // ── Шаг 2: Инициализация логгера ─────────────────────────────────────
    splash->updateStatus(25, QObject::tr("Инициализация логгера..."));
    splashDelay(60);
    Logger::instance().init();

    // ── Шаг 3: Загрузка перевода Qt базовых виджетов ─────────────────────
    splash->updateStatus(40, QObject::tr("Загрузка языкового пакета..."));
    splashDelay(60);
    const QString lang = SessionManager::instance().language();
    QTranslator qtTranslator;
    if (qtTranslator.load("qt_" + lang,
            QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // ── Шаг 4: Загрузка перевода приложения ──────────────────────────────
    splash->updateStatus(55, QObject::tr("Загрузка перевода приложения..."));
    splashDelay(60);
    loadTranslation(app, lang);

    // ── Шаг 5: Сбор системной информации ────────────────────────────────
    // (CPU, RAM, OS — передаётся собеседнику в HANDSHAKE и отображается в профиле)
    splash->updateStatus(70, QObject::tr("Сбор информации об устройстве..."));
    splashDelay(80);
    SystemInfo::instance().collect();

    // ── Настройка пула потоков под количество ядер CPU ────────────────────
    // 4–6 ядер → 4 воркера; более 6 → 6 воркеров; менее 4 → оставляем Qt-дефолт.
    // Ограничение предотвращает перегрузку ядер при параллельных операциях
    // (хеш файлов, шифрование, сеть) — QtConcurrent::run() использует этот пул.
    {
        const int cpuCores = QThread::idealThreadCount();
        int workerThreads  = cpuCores; // дефолт для <4 ядер
        if (cpuCores >= 4 && cpuCores <= 6)
            workerThreads = 4;
        else if (cpuCores > 6)
            workerThreads = 6;
        QThreadPool::globalInstance()->setMaxThreadCount(workerThreads);
        qDebug("[ThreadPool] Воркеров: %d (ядер CPU: %d)", workerThreads, cpuCores);
    }

    // ── Шаг 6: Инициализация базы данных и сети (через MainWindow) ────────
    splash->updateStatus(85, QObject::tr("Запуск сетевого модуля..."));
    splashDelay(80);

    // ── Шаг 7: Создание главного окна ─────────────────────────────────────
    splash->updateStatus(95, QObject::tr("Загрузка интерфейса..."));
    splashDelay(60);
    MainWindow w;

    // Финальный шаг — всё готово
    splash->updateStatus(100, QObject::tr("Готово!"));
    splashDelay(300);

    w.show();
    splash->close();
    splash->deleteLater();

    return app.exec();
}
