#ifndef FILEUPLOADMANAGER_H
#define FILEUPLOADMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QQueue>
#include <QFile>
#include <QNetworkAccessManager>
#include <QFileInfo>

#define IP              "8.136.113.168"
const QString SERVER_PATH = "http://8.136.113.168";
const QString PATH_PREFIX = "/uploads";
const QString FIRMWARE_PATH_PREFIX = "/firmwares";

enum TaskState {
    Pending,
    Uploading,
    Done,
    Failed
};

struct Task {

    QString     localPath;     // C:/Users/22824/Desktop/促销海报.jpg
    QString     fileName;      // 促销海报.jpg
    qint64      fileSize;      // 3145728
    QString     serverUrl;     // http://8.136.113.168/uploads/促销海报.jpg
    QString     serverPath;    // /uploads/促销海报.jpg
    qint64      resumePos;     // HEAD 拿到的 Content-Length，0=新文件
    TaskState   state;

    Task() {};
    explicit Task(QString &filePath, const QString &pathPrefix = PATH_PREFIX);
};

class FileUploadManager : public QObject
{
    Q_OBJECT
public:

    static FileUploadManager& getInstance();

    FileUploadManager(const FileUploadManager& other) = delete;
    void operator=(const FileUploadManager& other) = delete;

    Q_INVOKABLE void addTasks(QStringList list);

    Q_INVOKABLE void addTask(QString file);

    Q_INVOKABLE void addFirmwareTask(QString file);

    Q_INVOKABLE void requestFileMd5(const QString &file);

    void buildUploadTask(Task task);
signals:
    void fileMd5Computed(const QString &filePath, const QString &md5);
    void taskFailed(const QString& taskName, const QString& message);
    void taskStarted(const QString& taskName, int taskSize);
    void taskProgress(const QString& taskName, int sent, int total);
    void taskFinished(const QString& taskName, const QString& taskServerPath, const QString& taskServerUrl);
private:
    explicit FileUploadManager(QObject *parent = nullptr);

    void onUploadFinished(QNetworkReply* reply);

    QHash<QNetworkReply*, Task> m_active;  // 跟踪进行中的上传
};

#endif // FILEUPLOADMANAGER_H
