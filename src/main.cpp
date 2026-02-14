#include <QApplication>
#include <QFont>
#include <QNetworkProxy>
#include "ui/mainwindow.h"
#include "core/sessionmanager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("naleystogramm");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("naleystogramm");

    // Прямое подключение — системный прокси не используется
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    // Загружаем сессию до создания окна
    SessionManager::instance().load();

    QFont font;
    font.setFamily("Sans Serif");
    font.setPointSize(10);
    font.setStyleHint(QFont::SansSerif);
    app.setFont(font);

    MainWindow w;
    w.show();

    return app.exec();
}
