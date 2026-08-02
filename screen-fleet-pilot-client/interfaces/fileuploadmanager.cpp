#include "fileuploadmanager.h"
#include <QDebug>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QThread>

Task::Task(QString &filePath, const QString &pathPrefix)
{
    qDebug() << "[client]: construct task:" << filePath << "->" << pathPrefix;
    QFileInfo file(filePath);

    fileName   = file.fileName();
    fileSize   = file.size();
    resumePos  = 0;
    state      = Pending;
    localPath  = filePath;
    serverUrl  = SERVER_PATH + pathPrefix + "/" + fileName;
    serverPath = pathPrefix + "/" + fileName;
}

FileUploadManager::FileUploadManager(QObject *parent)
    : QObject{parent}
{}

void FileUploadManager::onUploadFinished(QNetworkReply *reply)
{
    const Task task = m_active.take(reply);
    if (reply->error() != QNetworkReply::NoError) {
        emit taskFailed(task.fileName, reply->errorString());
        reply->deleteLater();
        return;
    }
    const int status = reply->attribute(
                                QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        emit taskFailed(task.fileName,
                        QStringLiteral("HTTP %1").arg(status));
        reply->deleteLater();
        return;
    }
    emit taskFinished(task.fileName, task.serverPath, task.serverUrl);
    reply->deleteLater();
}

FileUploadManager &FileUploadManager::getInstance()
{
    static FileUploadManager instance;
    return instance;
}

void FileUploadManager::addTasks(QStringList list)
{
    for (QString& file : list) {

        QString localPath = QUrl(file).toLocalFile();

        QFileInfo fi(localPath);
        if (!fi.exists()) {
            qDebug() << "file not exists:" << localPath;
            continue;
        }
        qDebug() << "[client]: selected file: " << file;

        Task task(localPath);

        buildUploadTask(task);
    }
}

void FileUploadManager::addTask(QString file)
{
    QString localPath = QUrl(file).toLocalFile();
    if (localPath.isEmpty())
        localPath = file;

    QFileInfo fi(localPath);
    if (!fi.exists()) {
        qDebug() << "file not exists:" << localPath;
        return ;
    }
    qDebug() << "[client]: selected file: " << file;

    Task task(localPath);

    buildUploadTask(task);
}

void FileUploadManager::addFirmwareTask(QString file)
{
    QString localPath = QUrl(file).toLocalFile();
    if (localPath.isEmpty())
        localPath = file;

    QFileInfo fi(localPath);
    if (!fi.exists()) {
        qDebug() << "firmware not exists:" << localPath;
        return;
    }
    qDebug() << "[client]: upload firmware:" << localPath;

    Task task(localPath, FIRMWARE_PATH_PREFIX);
    buildUploadTask(task);
}

void FileUploadManager::requestFileMd5(const QString &file)
{
    QString localPath = QUrl(file).toLocalFile();
    if (localPath.isEmpty())
        localPath = file;

    QThread *thread = QThread::create([this, localPath]() {
        QString md5;
        QFile f(localPath);
        if (f.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Md5);
            while (!f.atEnd())
                hash.addData(f.read(1024 * 1024));
            md5 = QString::fromLatin1(hash.result().toHex());
        }
        QMetaObject::invokeMethod(this, [this, localPath, md5]() {
            emit fileMd5Computed(localPath, md5);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void FileUploadManager::buildUploadTask(Task task)
{
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QFile* file = new QFile(task.localPath);

    if (!file->open(QIODevice::ReadOnly)) {
        emit taskFailed(task.fileName, QStringLiteral("无法打开文件"));
        file->deleteLater();
        return;
    }

    QUrl url(task.serverUrl);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    req.setHeader(QNetworkRequest::ContentLengthHeader, task.fileSize);

    QNetworkReply* reply = manager->put(req, file);

    m_active.insert(reply, task);
    emit taskStarted(task.fileName, task.fileSize);
    connect(reply, &QNetworkReply::uploadProgress, this,
            [this, reply](qint64 sent, qint64 total) {
                const Task t = m_active.value(reply);
                emit taskProgress(t.fileName, sent, total);
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onUploadFinished(reply);
    });
}
