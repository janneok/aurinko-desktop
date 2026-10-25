#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QCoreApplication>
#include <QQmlContext>
#include <QIcon>

#include "SolarCalculator.h"
#include "Version.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Aurinko");
    QCoreApplication::setApplicationVersion(QString::fromLatin1(AURINKO_APP_VERSION));
    QCoreApplication::setOrganizationName("Oksanen");
    QGuiApplication::setDesktopFileName("fi.oksanen.Aurinko");
    // Sama kuvake jo käännetyn sovelluksen ikkunassa ennen KDE-asennusta.
    app.setWindowIcon(QIcon(QStringLiteral(":/resources/aurinko.png")));

    // QML:ssä piirretään omat tummat paneelit, Qt:n Basic-tyyli antaa
    // natiivin Qt Quick Controls -pohjan ilman ulkoisia teemapaketteja.
    QQuickStyle::setStyle("Basic");

    SolarCalculator solar;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("solar", &solar);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    engine.loadFromModule("Aurinko", "Main");
    return app.exec();
}
