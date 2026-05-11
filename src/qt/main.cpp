// Prevent SDL from redefining main
#define SDL_MAIN_HANDLED
#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QTranslator>
#include <QLocale>
#include <QSettings>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FCGo");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("FCGo");

    // Set application style
    app.setStyle("Fusion");

    // Load translation
    QSettings settings("FCGo", "FCGo");
    QString lang = settings.value("Language", "en").toString();
    
    if (lang == "zh_CN") {
        QTranslator* translator = new QTranslator(&app);
        if (translator->load(":/src/qt/translations/fcgo_zh_CN.qm")) {
            app.installTranslator(translator);
        }
    }

    MainWindow window;
    window.show();

    // Load ROM from command line
    QStringList args = app.arguments();
    if (args.size() > 1) {
        QString romPath = args.at(1);
        if (QFile::exists(romPath)) {
            window.loadROM(romPath);
        }
    }

    return app.exec();
}
