#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QIcon>
#include <QQuickStyle>
#include "interfaces/networkmanager.h"
#include "interfaces/fileuploadmanager.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Fusion");
    app.setWindowIcon(QIcon(":/resources/app-icon.svg"));

    NetworkManager networkManager;  // 栈上，app 析构前先析构
    FileUploadManager* fileUploadManager = &FileUploadManager::getInstance();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("NetworkManager", &networkManager);
    engine.rootContext()->setContextProperty("FileUploader", fileUploadManager);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    int ret = app.exec();
    return ret;
}
