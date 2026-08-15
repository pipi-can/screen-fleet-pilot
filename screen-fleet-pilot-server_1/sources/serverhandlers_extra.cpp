#include "../includes/serverparser.h"
#include "../includes/pushmgr.h"
#include "../includes/firmwaremgr.h"
#include "../includes/schedulemgr.h"
#include "../includes/global_def.h"
#include "logmgr.h"
#include "socketmgr.h"
#include <cstdlib>
#include <ctime>

static LogMgr* logger = &LogMgr::getInstance();

static bool isClockTrusted(time_t deviceTs) {
    if (deviceTs <= 0) return false;
    time_t now = time(nullptr);
    time_t diff = (now >= deviceTs) ? (now - deviceTs) : (deviceTs - now);
    return diff <= CLOCK_TRUST_SEC;
}

void RequestPushContentHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) return;
    if (!DeviceMgr::getInstance().isRegisteredClient(parserCtx.senderFd)) return;

    RequestPushContentBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "push content: params invalid", true);
        return;
    }

    for (const std::string& uid : bag.deviceUids) {
        PushMgr::getInstance().pushResourcesToEmbedded(uid, bag.paths);
    }
}

void RequestSchedulePushHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) return;
    if (!DeviceMgr::getInstance().isRegisteredClient(parserCtx.senderFd)) return;

    RequestSchedulePushBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "schedule push: params invalid", true);
        return;
    }

    time_t triggerAt = ScheduleMgr::parseTriggerAt(bag.scheduleDate, bag.scheduleTime);
    if (triggerAt <= 0) {
        logger->logMsg(ERROR, "schedule push: invalid trigger time", true);
        return;
    }

    for (const std::string& uid : bag.deviceUids) {
        int embeddedFd = DeviceMgr::getInstance().getEmbeddedFdByUid(uid);
        if (embeddedFd < 0) {
            logger->logMsg(WARNING, "schedule push: offline uid=" + uid, true);
            continue;
        }
        OnlineClientInfo* info = DeviceMgr::getInstance().getClient(embeddedFd);

        if (info && isClockTrusted(info->deviceTimestamp)) {
            PushSchedulePlaylistBag push;
            push.paths = PushMgr::toHttpUrls(bag.paths);
            push.scheduleDate = bag.scheduleDate;
            push.scheduleTime = bag.scheduleTime;
            push.durationSec = bag.durationSec;
            PushMgr::getInstance().pushSchedulePlaylist(uid, push);
        } else {
            ScheduleTask task;
            task.deviceUid = uid;
            task.scheduleDate = bag.scheduleDate;
            task.scheduleTime = bag.scheduleTime;
            task.durationSec = bag.durationSec;
            task.triggerAt = triggerAt;
            task.paths = bag.paths;
            task.enabled = true;
            ScheduleMgr::getInstance().addTask(task);
        }
    }
}

void RequestFirmwareListHandler::action(const ParserContext parserCtx) {
    if (!DeviceMgr::getInstance().isRegisteredClient(parserCtx.senderFd)) return;

    RequestFirmwareListAckBag ack;
    FirmwareMgr::getInstance().loadFirmwareList(ack.firmwares);
    ack.count = static_cast<int>(ack.firmwares.size());

    char* jsonStr = ack.toJsonString();
    if (!jsonStr) return;
    std::string json(jsonStr);
    free(jsonStr);
    SocketMgr::sendMessage(parserCtx.senderFd, json);
}

void RequestOtaUpdateHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) return;
    if (!DeviceMgr::getInstance().isRegisteredClient(parserCtx.senderFd)) return;

    RequestOtaUpdateBag bag;
    bag.loadFromJsonString(parserCtx.message);
    OnlineClientInfo* clientInfo = DeviceMgr::getInstance().getClient(parserCtx.senderFd);
    if (clientInfo && bag.clientUid.empty()) {
        bag.clientUid = clientInfo->deviceUid;
    }
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "ota update: params invalid", true);
        return;
    }

    std::string firmwarePath;
    if (!FirmwareMgr::getInstance().resolveFirmwarePath(bag.path, firmwarePath)) {
        logger->logMsg(ERROR, "ota update: firmware not found " + bag.path, true);
        return;
    }
    std::string md5 = FirmwareMgr::getInstance().computeFileMd5(firmwarePath);
    if (md5.empty()) {
        logger->logMsg(ERROR, "ota update: md5 failed " + bag.path, true);
        return;
    }

    for (const std::string& uid : bag.deviceUids) {
        int embeddedFd = DeviceMgr::getInstance().getEmbeddedFdByUid(uid);
        if (embeddedFd < 0) {
            logger->logMsg(WARNING, "ota update: offline uid=" + uid, true);
            continue;
        }
        OtaUpdateBag ota;
        ota.path = bag.path;
        ota.md5 = md5;
        ota.clientUid = bag.clientUid;
        char* jsonStr = ota.toJsonString();
        if (!jsonStr) continue;
        std::string json(jsonStr);
        free(jsonStr);
        SocketMgr::sendMessage(embeddedFd, json);
    }
}

void EmbeddedOtaUpdateAckHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) return;

    EmbeddedOtaUpdateAckBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "ota_update_ack: params invalid", true);
        return;
    }

    int clientFd = DeviceMgr::getInstance().getClientFdByUid(bag.clientUid);
    if (clientFd < 0) {
        logger->logMsg(WARNING, "ota_update_ack: client offline uid=" + bag.clientUid, true);
        return;
    }

    ClientOtaUpdateAckBag ack;
    ack.result = (bag.result == 1) ? 1 : 0;
    if (!bag.path.empty()) ack.path = bag.path;

    char* jsonStr = ack.toJsonString();
    if (!jsonStr) return;
    std::string json(jsonStr);
    free(jsonStr);
    SocketMgr::sendMessage(clientFd, json);
}

void RequestCheckFirmwareHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) return;
    if (!DeviceMgr::getInstance().isRegisteredClient(parserCtx.senderFd)) return;

    RequestCheckFirmwareBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "check firmware: params invalid", true);
        return;
    }

    CheckFirmwareAckBag ack;
    ack.result = FirmwareMgr::getInstance().checkFirmwareMd5(bag.path, bag.md5) ? 1 : 0;

    char* jsonStr = ack.toJsonString();
    if (!jsonStr) return;
    std::string json(jsonStr);
    free(jsonStr);
    SocketMgr::sendMessage(parserCtx.senderFd, json);
}
