#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "interfaces/contentplayer.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("screen-fleet-pilot");
    QGuiApplication::setApplicationVersion("1.0");

    ContentPlayer& player = ContentPlayer::getInstance();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("player", &player);

    const QUrl url(QStringLiteral("qrc:/main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
