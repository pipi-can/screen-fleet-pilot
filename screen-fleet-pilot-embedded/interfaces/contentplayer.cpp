#include "contentplayer.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QDebug>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QDateTime>
#include <QDir>

static const char* kImageSuffixes[] = {
    ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", nullptr
};

ContentPlayer& ContentPlayer::getInstance()
{
    static ContentPlayer instance;
    return instance;
}

ContentPlayer::ContentPlayer(QObject* parent)
    : QObject(parent)
{
    connect(&m_slideTimer, &QTimer::timeout, this, &ContentPlayer::onSlideTimer);
    m_slideTimer.setInterval(SLIDE_INTERVAL_MS);

    connect(&m_schedulePollTimer, &QTimer::timeout, this, &ContentPlayer::onSchedulePoll);
    m_schedulePollTimer.setInterval(SCHEDULE_POLL_MS);

    startListening();
    loadPlaylistFile();
    loadScheduleFile();
    m_schedulePollTimer.start();
    checkSchedulePlayback();
}

bool ContentPlayer::isImagePath(const QString& path)
{
    const QString lower = path.toLower();
    for (int i = 0; kImageSuffixes[i]; ++i) {
        if (lower.endsWith(QLatin1String(kImageSuffixes[i]))) {
            return true;
        }
    }
    return false;
}

QString ContentPlayer::toFileUrl(const QString& path)
{
    return QUrl::fromLocalFile(path).toString();
}

QStringList ContentPlayer::filterImagePaths(const QStringList& paths)
{
    QStringList images;
    for (const QString& p : paths) {
        if (isImagePath(p)) {
            images.append(p);
        }
    }
    return images;
}

QDateTime ContentPlayer::parseScheduleDateTime(const QString& date, const QString& time)
{
    const QString combined = date.trimmed() + QLatin1Char(' ') + time.trimmed();
    QDateTime dt = QDateTime::fromString(combined, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid()) {
        dt = QDateTime::fromString(combined, QStringLiteral("yyyy-MM-dd HH:mm"));
    }
    return dt;
}

void ContentPlayer::startListening()
{
    QLocalServer::removeServer(QLatin1String(SOCKET_PATH));

    if (!m_server.listen(QLatin1String(SOCKET_PATH))) {
        m_statusInfo = QStringLiteral("socket listen failed: %1").arg(m_server.errorString());
        emit statusInfoChanged();
        qWarning() << "[player]" << m_statusInfo;
        return;
    }

    connect(&m_server, &QLocalServer::newConnection, this, &ContentPlayer::onNewConnection);
    m_statusInfo = QStringLiteral("listening on %1").arg(QLatin1String(SOCKET_PATH));
    emit statusInfoChanged();
    qDebug() << "[player]" << m_statusInfo;
}

void ContentPlayer::loadPlaylistFile()
{
    QFile file(QLatin1String(PLAYLIST_JSON_PATH));
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[player] no playlist file:" << PLAYLIST_JSON_PATH;
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qWarning() << "[player] invalid playlist json";
        return;
    }

    handleMessage(doc.object());
    qDebug() << "[player] loaded playlist from" << PLAYLIST_JSON_PATH;
}

void ContentPlayer::loadScheduleFile()
{
    QFile file(QLatin1String(SCHEDULE_JSON_PATH));
    if (!file.open(QIODevice::ReadOnly)) {
        m_hasSchedule = false;
        qDebug() << "[player] no schedule file:" << SCHEDULE_JSON_PATH;
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        m_hasSchedule = false;
        qWarning() << "[player] invalid schedule json";
        return;
    }

    const QJsonObject obj = doc.object();
    const QString date = obj.value(QStringLiteral("schedule_date")).toString();
    const QString time = obj.value(QStringLiteral("schedule_time")).toString();
    const int durationSec = obj.value(QStringLiteral("duration_sec")).toInt(0);

    m_scheduleStart = parseScheduleDateTime(date, time);
    if (!m_scheduleStart.isValid() || durationSec <= 0) {
        m_hasSchedule = false;
        qWarning() << "[player] invalid schedule time or duration"
                   << date << time << durationSec;
        return;
    }
    m_scheduleEnd = m_scheduleStart.addSecs(durationSec);

    QStringList paths;
    const QJsonArray localArr = obj.value(QStringLiteral("local_paths")).toArray();
    for (const QJsonValue& v : localArr) {
        const QString p = v.toString();
        if (!p.isEmpty()) {
            paths.append(p);
        }
    }
    if (paths.isEmpty()) {
        const QJsonArray urlArr = obj.value(QStringLiteral("paths")).toArray();
        for (const QJsonValue& v : urlArr) {
            const QString url = v.toString();
            const int slash = url.lastIndexOf(QLatin1Char('/'));
            if (slash >= 0 && slash + 1 < url.size()) {
                paths.append(QStringLiteral(CONTENT_DIR) + QLatin1Char('/')
                             + url.mid(slash + 1));
            }
        }
    }

    m_scheduleImagePaths = filterImagePaths(paths);
    m_hasSchedule = !m_scheduleImagePaths.isEmpty();

    qDebug() << "[player] schedule loaded:"
             << "start=" << m_scheduleStart.toString(Qt::ISODate)
             << "end=" << m_scheduleEnd.toString(Qt::ISODate)
             << "images=" << m_scheduleImagePaths.size();
}

void ContentPlayer::handleScheduleReady()
{
    qDebug() << "[player] schedule_ready from agent";
    loadScheduleFile();
    checkSchedulePlayback();
}

void ContentPlayer::onSchedulePoll()
{
    checkSchedulePlayback();
}

void ContentPlayer::checkSchedulePlayback()
{
    if (!m_hasSchedule) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();

    if (m_playingSchedule) {
        if (now >= m_scheduleEnd) {
            qDebug() << "[player] schedule ended, restore default playlist";
            restoreDefaultPlaylist();
        }
        return;
    }

    if (now >= m_scheduleStart && now < m_scheduleEnd) {
        qDebug() << "[player] schedule window active, start schedule playlist";
        startSchedulePlayback();
    }
}

void ContentPlayer::startSchedulePlayback()
{
    if (m_scheduleImagePaths.isEmpty()) {
        return;
    }

    m_playingSchedule = true;
    setPlaylist(m_scheduleImagePaths, false);

    m_statusInfo = QStringLiteral("schedule %1 image(s) until %2")
            .arg(m_scheduleImagePaths.size())
            .arg(m_scheduleEnd.toString(QStringLiteral("HH:mm:ss")));
    emit statusInfoChanged();
}

void ContentPlayer::restoreDefaultPlaylist()
{
    m_playingSchedule = false;

    if (m_defaultImagePaths.isEmpty()) {
        m_statusInfo = QStringLiteral("schedule ended, no default playlist");
        emit statusInfoChanged();
        qDebug() << "[player] schedule ended, default playlist empty";
        return;
    }

    setPlaylist(m_defaultImagePaths, false);
    qDebug() << "[player] restored default playlist," << m_defaultImagePaths.size() << "image(s)";
}

void ContentPlayer::handleMessage(const QJsonObject& obj)
{
    const QString cmd = obj.value(QStringLiteral("cmd")).toString();
    if (cmd != QLatin1String("content_ready")) {
        qWarning() << "[player] ignore cmd:" << cmd;
        return;
    }

    QStringList paths;
    const QJsonArray arr = obj.value(QStringLiteral("paths")).toArray();
    for (const QJsonValue& v : arr) {
        const QString p = v.toString();
        if (!p.isEmpty()) {
            paths.append(p);
        }
    }

    setPlaylist(paths, true);
}

void ContentPlayer::handleScreenshotRequest(int deviceId, QLocalSocket* socket)
{
    if (!socket) {
        return;
    }

    QWindow* win = QGuiApplication::focusWindow();
    if (!win) {
        const auto wins = QGuiApplication::allWindows();
        if (!wins.isEmpty()) {
            win = wins.first();
        }
    }

    QQuickWindow* qw = qobject_cast<QQuickWindow*>(win);
    if (!qw) {
        qWarning() << "[player] screenshot: no window";
        return;
    }

    const QImage img = qw->grabWindow();
    if (img.isNull()) {
        qWarning() << "[player] screenshot: grab failed";
        return;
    }

    QDir().mkpath(QStringLiteral(CONTENT_DIR));
    const QString path = QStringLiteral(CONTENT_DIR) + QStringLiteral("/screenshot_")
            + QString::number(QDateTime::currentMSecsSinceEpoch()) + QStringLiteral(".png");
    if (!img.save(path)) {
        qWarning() << "[player] screenshot: save failed" << path;
        return;
    }

    QJsonObject reply;
    reply.insert(QStringLiteral("cmd"), QStringLiteral("screenshot_ready"));
    reply.insert(QStringLiteral("device_id"), deviceId);
    reply.insert(QStringLiteral("path"), path);

    QByteArray line = QJsonDocument(reply).toJson(QJsonDocument::Compact);
    line.append('\n');
    socket->write(line);
    socket->waitForBytesWritten(3000);

    qDebug() << "[player] screenshot saved" << path << "device_id=" << deviceId;
}

void ContentPlayer::setPlaylist(const QStringList& paths, bool asDefault)
{
    const QStringList images = filterImagePaths(paths);
    if (images.isEmpty()) {
        qWarning() << "[player] no image in playlist";
        return;
    }

    if (asDefault) {
        m_defaultImagePaths = images;
        if (m_playingSchedule) {
            qDebug() << "[player] content_ready saved as default, schedule still playing";
            return;
        }
    }

    m_imagePaths = images;
    m_currentIndex = 0;
    emit playlistChanged();

    showImageAt(m_currentIndex);

    if (!m_slideTimer.isActive()) {
        m_slideTimer.start();
    }

    if (m_playingSchedule) {
        m_statusInfo = QStringLiteral("schedule playing %1 image(s)").arg(m_imagePaths.size());
    } else {
        m_statusInfo = QStringLiteral("playing %1 image(s)").arg(m_imagePaths.size());
    }
    emit statusInfoChanged();
    qDebug() << "[player] playlist updated," << m_imagePaths.size() << "image(s)";
}

void ContentPlayer::showImageAt(int index)
{
    if (m_imagePaths.isEmpty()) {
        return;
    }

    if (index < 0 || index >= m_imagePaths.size()) {
        index = 0;
    }

    m_currentIndex = index;
    m_currentImage = toFileUrl(m_imagePaths.at(index));
    emit currentIndexChanged();
    emit currentImageChanged();
    qDebug() << "[player] show" << (index + 1) << "/" << m_imagePaths.size() << m_currentImage;
}

void ContentPlayer::onSlideTimer()
{
    if (m_imagePaths.size() <= 1) {
        return;
    }
    const int next = (m_currentIndex + 1) % m_imagePaths.size();
    showImageAt(next);
}

void ContentPlayer::onNewConnection()
{
    QLocalSocket* socket = m_server.nextPendingConnection();
    if (!socket) {
        return;
    }

    if (m_agentSocket) {
        m_agentSocket->disconnect(this);
        m_agentSocket->deleteLater();
    }
    m_agentSocket = socket;

    connect(m_agentSocket, &QLocalSocket::readyRead, this, &ContentPlayer::onSocketReadyRead);
    connect(m_agentSocket, &QLocalSocket::disconnected, this, [this]() {
        qWarning() << "[player] agent disconnected";
        m_agentSocket = nullptr;
    });
    m_agentSocket->setProperty("buffer", QByteArray());
    qDebug() << "[player] agent connected";
}

void ContentPlayer::onSocketReadyRead()
{
    QLocalSocket* socket = m_agentSocket;
    if (!socket) {
        return;
    }

    QByteArray buffer = socket->property("buffer").toByteArray();
    buffer.append(socket->readAll());

    while (true) {
        const int nl = buffer.indexOf('\n');
        if (nl < 0) {
            break;
        }

        QByteArray line = buffer.left(nl).trimmed();
        buffer.remove(0, nl + 1);
        if (line.isEmpty()) {
            continue;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            const QString cmd = obj.value(QStringLiteral("cmd")).toString();
            if (cmd == QLatin1String("screenshot_request")) {
                handleScreenshotRequest(obj.value(QStringLiteral("device_id")).toInt(-1), socket);
            } else if (cmd == QLatin1String("schedule_ready")) {
                handleScheduleReady();
            } else {
                handleMessage(obj);
            }
        } else {
            qWarning() << "[player] invalid socket json:" << line;
        }
    }

    socket->setProperty("buffer", buffer);
}
