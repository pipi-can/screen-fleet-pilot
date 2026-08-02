#ifndef CONTENTPLAYER_H
#define CONTENTPLAYER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QStringList>
#include <QDateTime>

#define CONTENT_DIR           "/var/lib/device-agent/content"
#define PLAYLIST_JSON_PATH    CONTENT_DIR "/playlist.json"
#define SCHEDULE_JSON_PATH    CONTENT_DIR "/schedule.json"
#define SOCKET_PATH           "/tmp/screen_fleet_pilot.sock"
#define SLIDE_INTERVAL_MS     10000
#define SCHEDULE_POLL_MS      1000

class ContentPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentImage READ currentImage NOTIFY currentImageChanged)
    Q_PROPERTY(QString statusInfo READ statusInfo NOTIFY statusInfoChanged)
    Q_PROPERTY(int imageCount READ imageCount NOTIFY playlistChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)

public:
    static ContentPlayer& getInstance();

    QString currentImage() const { return m_currentImage; }
    QString statusInfo() const { return m_statusInfo; }
    int imageCount() const { return m_imagePaths.size(); }
    int currentIndex() const { return m_currentIndex; }

signals:
    void currentImageChanged();
    void statusInfoChanged();
    void playlistChanged();
    void currentIndexChanged();

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSlideTimer();
    void onSchedulePoll();

private:
    explicit ContentPlayer(QObject* parent = nullptr);

    void startListening();
    void loadPlaylistFile();
    void loadScheduleFile();
    void checkSchedulePlayback();
    void startSchedulePlayback();
    void restoreDefaultPlaylist();
    void handleMessage(const QJsonObject& obj);
    void handleScheduleReady();
    void handleScreenshotRequest(int deviceId, QLocalSocket* socket);
    void setPlaylist(const QStringList& paths, bool asDefault);
    void showImageAt(int index);
    static bool isImagePath(const QString& path);
    static QString toFileUrl(const QString& path);
    static QStringList filterImagePaths(const QStringList& paths);
    static QDateTime parseScheduleDateTime(const QString& date, const QString& time);

    QLocalServer m_server;
    QLocalSocket* m_agentSocket = nullptr;
    QTimer       m_slideTimer;
    QTimer       m_schedulePollTimer;
    QStringList  m_imagePaths;
    QStringList  m_defaultImagePaths;
    QStringList  m_scheduleImagePaths;
    QString      m_currentImage;
    QString      m_statusInfo;
    int          m_currentIndex = 0;
    bool         m_playingSchedule = false;
    bool         m_hasSchedule = false;
    QDateTime    m_scheduleStart;
    QDateTime    m_scheduleEnd;
};

#endif
