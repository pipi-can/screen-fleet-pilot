#include "../includes/contentmgr.h"
#include "../includes/client.h"
#include "logmgr.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <strings.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

extern "C" {
#include <json-c/json.h>
}

static LogMgr* logger = &LogMgr::getInstance();

struct DownloadContext {
    std::vector<std::string> urls;
    bool notifyContentReady;
    PushSchedulePlaylistBag scheduleBag;
};

static std::string toHttpUrl(const std::string& relativePath) {
    if (relativePath.rfind("http://", 0) == 0) return relativePath;
    std::string path = relativePath;
    if (path.empty() || path[0] != '/') path = "/" + path;
    return std::string("http://") + SERVER_HOST + path;
}

static void* downloadThread(void* arg) {
    DownloadContext* ctx = static_cast<DownloadContext*>(arg);
    ContentMgr& mgr = ContentMgr::getInstance();
    std::vector<std::string> locals;

    system("mkdir -p " CONTENT_DIR);
    for (const std::string& url : ctx->urls) {
        std::string local = mgr.urlToLocalPath(url);
        if (local.empty()) continue;
        if (access(local.c_str(), F_OK) == 0) {
            locals.push_back(local);
            continue;
        }
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "curl -fsSL -o '%s' '%s'", local.c_str(), url.c_str());
        if (system(cmd) == 0) {
            locals.push_back(local);
        }
    }

    if (ctx->notifyContentReady && !locals.empty()) {
        mgr.writePlaylistJson(locals);
        mgr.notifyDownloadReady("content_ready");
    } else if (!ctx->notifyContentReady) {
        mgr.writeScheduleJson(ctx->scheduleBag, locals);
        mgr.notifyDownloadReady("schedule_ready");
    }

    delete ctx;
    return nullptr;
}

std::string ContentMgr::urlToLocalPath(const std::string& url) const {
    size_t pos = url.rfind('/');
    if (pos == std::string::npos) return "";
    return std::string(CONTENT_DIR) + "/" + url.substr(pos + 1);
}

bool ContentMgr::connectToQt() {
    if (m_qtFd >= 0) return true;
    m_qtFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_qtFd < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(m_qtFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(m_qtFd);
        m_qtFd = -1;
        return false;
    }
    logger->logMsg(DEBUG, "connected to qt player", true);
    return true;
}

void ContentMgr::onQtDisconnected() {
    if (m_qtFd >= 0) {
        close(m_qtFd);
        m_qtFd = -1;
    }
}

bool ContentMgr::notifyQt(const char* jsonLine) {
    if (!jsonLine || m_qtFd < 0) return false;
    size_t len = strlen(jsonLine);
    return send(m_qtFd, jsonLine, len, MSG_NOSIGNAL) == static_cast<ssize_t>(len);
}

void ContentMgr::notifyDownloadReady(const char* cmd) {
    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "cmd", json_object_new_string(cmd));
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    std::string line = std::string(jsonStr) + "\n";
    json_object_put(root);
    notifyQt(line.c_str());
}

void ContentMgr::startDownload(const std::vector<std::string>& urls, bool notifyContentReady) {
    if (urls.empty()) return;
    DownloadContext* ctx = new DownloadContext{urls, notifyContentReady};
    pthread_t tid;
    if (pthread_create(&tid, nullptr, downloadThread, ctx) == 0) {
        pthread_detach(tid);
    } else {
        delete ctx;
    }
}

bool ContentMgr::writePlaylistJson(const std::vector<std::string>& localPaths) {
    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "cmd", json_object_new_string("content_playlist"));
    struct json_object* arr = json_object_new_array();
    for (const std::string& path : localPaths) {
        json_object_array_add(arr, json_object_new_string(path.c_str()));
    }
    json_object_object_add(root, "local_paths", arr);

    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    FILE* fp = fopen(PLAYLIST_JSON_PATH, "w");
    if (!fp) {
        json_object_put(root);
        return false;
    }
    fprintf(fp, "%s\n", jsonStr);
    fclose(fp);
    json_object_put(root);
    return true;
}

bool ContentMgr::writeScheduleJson(const PushSchedulePlaylistBag& bag,
                                   const std::vector<std::string>& localPaths) {
    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "cmd", json_object_new_string("schedule_playlist"));
    json_object_object_add(root, "schedule_date", json_object_new_string(bag.scheduleDate.c_str()));
    json_object_object_add(root, "schedule_time", json_object_new_string(bag.scheduleTime.c_str()));
    json_object_object_add(root, "duration_sec", json_object_new_int(bag.durationSec));
    struct json_object* arr = json_object_new_array();
    for (const std::string& path : localPaths) {
        json_object_array_add(arr, json_object_new_string(path.c_str()));
    }
    json_object_object_add(root, "local_paths", arr);

    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    FILE* fp = fopen(SCHEDULE_JSON_PATH, "w");
    if (!fp) {
        json_object_put(root);
        return false;
    }
    fprintf(fp, "%s\n", jsonStr);
    fclose(fp);
    json_object_put(root);
    return true;
}

void ContentMgr::handlePushResources(const PushResourcesToDownloadBag& bag) {
    startDownload(bag.paths, true);
}

void ContentMgr::handlePushSchedule(const PushSchedulePlaylistBag& bag) {
    DownloadContext* ctx = new DownloadContext{bag.paths, false, bag};
    pthread_t tid;
    if (pthread_create(&tid, nullptr, downloadThread, ctx) == 0) {
        pthread_detach(tid);
    } else {
        delete ctx;
    }
}

void ContentMgr::handleScreenshotRequest(int requestClientFd) {
    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "cmd", json_object_new_string("screenshot_request"));
    json_object_object_add(root, "device_id", json_object_new_int(requestClientFd));
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    std::string line = std::string(jsonStr) + "\n";
    json_object_put(root);
    notifyQt(line.c_str());
}

bool ContentMgr::uploadScreenshot(const std::string& localPath, std::string& serverPath) {
    char remoteName[192];
    snprintf(remoteName, sizeof(remoteName), "%s_%ld.png",
             Client::getInstance().getDeviceUid().c_str(), static_cast<long>(time(nullptr)));
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -fsSL -T '%s' 'http://%s%s/%s'",
             localPath.c_str(), SERVER_HOST, SCREENSHOT_URL_PREFIX, remoteName);
    if (system(cmd) != 0) return false;
    serverPath = std::string(SCREENSHOT_URL_PREFIX) + "/" + remoteName;
    return true;
}

void ContentMgr::sendScreenshotData(int requestClientFd, const std::string& serverPath) {
    ScreenshotDataBag bag;
    bag.requestClientFd = requestClientFd;
    bag.path = serverPath;
    char* jsonStr = bag.toJsonString();
    if (!jsonStr) return;
    Client::getInstance().sendMessageToServer(jsonStr);
    free(jsonStr);
}

void ContentMgr::sendOtaAck(const EmbeddedOtaUpdateAckBag& ack) {
    EmbeddedOtaUpdateAckBag copy = ack;
    char* jsonStr = copy.toJsonString();
    if (!jsonStr) return;
    Client::getInstance().sendMessageToServer(jsonStr);
    free(jsonStr);
}

void ContentMgr::handleOtaUpdate(const OtaUpdateBag& bag) {
    EmbeddedOtaUpdateAckBag failAck;
    failAck.clientUid = bag.clientUid;
    failAck.code = -1;
    failAck.result = 0;

    if (bag.path.empty() || bag.md5.empty()) {
        failAck.msg = "invalid params";
        sendOtaAck(failAck);
        return;
    }

    std::string url = toHttpUrl(bag.path);
    system("mkdir -p " FIRMWARE_DIR);
    std::string localPath = std::string(FIRMWARE_DIR) + "/" + bag.path.substr(bag.path.rfind('/') + 1);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -fsSL -o '%s' '%s'", localPath.c_str(), url.c_str());

    if (system(cmd) != 0) {
        failAck.msg = "download failed";
        sendOtaAck(failAck);
        return;
    }

    char md5cmd[512];
    snprintf(md5cmd, sizeof(md5cmd), "md5sum '%s'", localPath.c_str());
    FILE* fp = popen(md5cmd, "r");
    char line[128] = {0};
    if (!fp || !fgets(line, sizeof(line), fp)) {
        if (fp) pclose(fp);
        failAck.msg = "md5 failed";
        sendOtaAck(failAck);
        return;
    }
    pclose(fp);

    char actualMd5[33] = {0};
    strncpy(actualMd5, line, 32);
    if (strncasecmp(actualMd5, bag.md5.c_str(), 32) != 0) {
        failAck.msg = "md5 mismatch";
        sendOtaAck(failAck);
        unlink(localPath.c_str());
        return;
    }

    EmbeddedOtaUpdateAckBag okAck;
    okAck.clientUid = bag.clientUid;
    okAck.code = 0;
    okAck.result = 1;
    okAck.msg = "ok";
    okAck.path = bag.path;
    okAck.localPath = localPath;

    char scriptCmd[512];
    snprintf(scriptCmd, sizeof(scriptCmd), "python3 %s '%s'", OTA_SCRIPT_PATH, localPath.c_str());
    int scriptRet = system(scriptCmd);
    if (scriptRet != 0) {
        okAck.result = 0;
        okAck.msg = "ota script failed";
    }
    sendOtaAck(okAck);
}

bool ContentMgr::handleQtMessage(const char* line) {
    struct json_object* root = json_tokener_parse(line);
    if (!root) return false;

    struct json_object* cmdObj = nullptr;
    if (!json_object_object_get_ex(root, "cmd", &cmdObj)) {
        json_object_put(root);
        return false;
    }
    const char* cmd = json_object_get_string(cmdObj);

    if (strcmp(cmd, "screenshot_ready") == 0) {
        struct json_object* idObj = nullptr;
        struct json_object* pathObj = nullptr;
        json_object_object_get_ex(root, "device_id", &idObj);
        json_object_object_get_ex(root, "path", &pathObj);
        int requestClientFd = idObj ? json_object_get_int(idObj) : -1;
        const char* path = pathObj ? json_object_get_string(pathObj) : nullptr;
        std::string serverPath;
        if (path && uploadScreenshot(path, serverPath)) {
            sendScreenshotData(requestClientFd, serverPath);
        }
    }

    json_object_put(root);
    return true;
}
