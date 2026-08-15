#include "../includes/pushmgr.h"
#include "socketmgr.h"
#include "devicemgr.h"
#include "logmgr.h"
#include "global_def.h"
#include <cstdlib>

static LogMgr* logger = &LogMgr::getInstance();

std::string PushMgr::toHttpUrl(const std::string& relativePath) {
    if (relativePath.empty()) {
        return "";
    }
    if (relativePath.rfind("http://", 0) == 0 || relativePath.rfind("https://", 0) == 0) {
        return relativePath;
    }
    std::string path = relativePath;
    if (path[0] != '/') {
        path = "/" + path;
    }
    return std::string("http://") + SERVER_HOST + path;
}

std::vector<std::string> PushMgr::toHttpUrls(const std::vector<std::string>& relativePaths) {
    std::vector<std::string> urls;
    urls.reserve(relativePaths.size());
    for (const std::string& path : relativePaths) {
        urls.push_back(toHttpUrl(path));
    }
    return urls;
}

bool PushMgr::pushResourcesToEmbedded(const std::string& uid,
                                      const std::vector<std::string>& relativePaths) {
    if (relativePaths.empty()) {
        return false;
    }

    int embeddedFd = DeviceMgr::getInstance().getEmbeddedFdByUid(uid);
    if (embeddedFd < 0) {
        logger->logMsg(WARNING, "push resources: device offline uid=" + uid, true);
        return false;
    }

    PushResourcesToDownloadBag push;
    push.paths = toHttpUrls(relativePaths);

    char* jsonStr = push.toJsonString();
    if (!jsonStr) {
        return false;
    }
    std::string json(jsonStr);
    free(jsonStr);

    if (!SocketMgr::sendMessage(embeddedFd, json)) {
        logger->logMsg(ERROR, "push resources send failed uid=" + uid, true);
        return false;
    }

    logger->logMsg(DEBUG, "push resources sent uid=" + uid, true);
    return true;
}

bool PushMgr::pushSchedulePlaylist(const std::string& uid, const PushSchedulePlaylistBag& bag) {
    int embeddedFd = DeviceMgr::getInstance().getEmbeddedFdByUid(uid);
    if (embeddedFd < 0) {
        logger->logMsg(WARNING, "push schedule: device offline uid=" + uid, true);
        return false;
    }

    char* jsonStr = bag.toJsonString();
    if (!jsonStr) {
        return false;
    }
    std::string json(jsonStr);
    free(jsonStr);

    if (!SocketMgr::sendMessage(embeddedFd, json)) {
        logger->logMsg(ERROR, "push schedule send failed uid=" + uid, true);
        return false;
    }

    logger->logMsg(DEBUG, "push schedule sent uid=" + uid, true);
    return true;
}
