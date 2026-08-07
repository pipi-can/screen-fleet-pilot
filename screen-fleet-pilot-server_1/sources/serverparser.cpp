#include "../includes/serverparser.h"
#include "logmgr.h"
#include "socketmgr.h"
#include "filelistmgr.h"
#include <cstdlib>
#include <ctime>
#include <string>

static LogMgr* logger = &LogMgr::getInstance();
static DatabaseMgr* dbMgr = &DatabaseMgr::getInstance();

void ServerParser::parseMessage(ParserContext parserCtx) {
    JsonBagBasic& basic = parserCtx.basic;
    JsonBagHandler* handler = nullptr;
    if (basic.source == "embedded") {
        if (basic.cmd == "register") {
            handler = new RegisterHandler();
            handler->action(parserCtx);
            delete handler;
        } else if (basic.cmd == "heartbeat") {
            handler = new EmbeddedHeartbeatHandler();
            handler->action(parserCtx);
            delete handler;
        } else if (basic.cmd == "update_info_ack") {
            handler = new UpdateInfoAckHandler();
            handler->action(parserCtx);
            delete handler;
        }
    } else if (basic.source == "client") {
        if (basic.cmd == "fetch_devices") {
            handler = new FetchDeviceHandler();
            handler->action(parserCtx);
            delete handler;
        } else if (basic.cmd == "request_update_embedded") {
            handler = new RequestUpdateEmbeddedHandler();
            handler->action(parserCtx);
            delete handler;
        } else if (basic.cmd == "request_file_list") {
            handler = new RequestFileListHandler();
            handler->action(parserCtx);
            delete handler;
        }
    }
}

static OnlineClientInfo makeOnlineInfo(const RegisterBag& bag) {
    OnlineClientInfo info;
    info.type    = bag.source == "embedded" ? ClientType::Embedded : ClientType::Client;
    info.name    = bag.name;
    info.group   = bag.group;
    info.version = bag.version;
    info.deviceUid = bag.deviceUid;
    info.lastHeartbeatTimestamp = time(nullptr);
    return info;
}

void RegisterHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) {
        logger->logMsg(ERROR, "register: empty message", true);
        return;
    }

    RegisterBag bag;
    bag.loadFromJsonString(parserCtx.message);

    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "register: params invalid or incomplete", true);
        return;
    }

    RegisterContext ctx;
    ctx.clientFd = parserCtx.senderFd;

    if (dbMgr->deviceExists(bag.deviceUid)) {
        dbMgr->updateDevice(bag.deviceUid, bag.name, bag.group);
        ctx.code = 1;
        ctx.msg  = "repeated";
        DeviceMgr::getInstance().addOnlineEmbeddedInfo(ctx.clientFd, makeOnlineInfo(bag));
        reply(ctx);
        return;
    }

    if (!dbMgr->insertDevice(bag.deviceUid, bag.name, bag.group, bag.source)) {
        ctx.code = -1;
        ctx.msg  = "error";
        reply(ctx);
        logger->logMsg(ERROR, "register: device register failed, uid: " + bag.deviceUid, true);
        return;
    }

    ctx.code = 0;
    ctx.msg  = "ok";
    DeviceMgr::getInstance().addOnlineEmbeddedInfo(ctx.clientFd, makeOnlineInfo(bag));
    reply(ctx);
    logger->logMsg(DEBUG, "register: device register success, uid: " + bag.deviceUid, true);
}

void RegisterHandler::reply(const RegisterContext registerCtx) {
    RegisterAckBag ack;
    ack.code    = registerCtx.code;
    ack.message = registerCtx.msg;

    char* jsonStr = ack.toJsonString();
    if (!jsonStr) {
        logger->logMsg(ERROR, "register reply: build json failed", true);
        return;
    }

    std::string json(jsonStr);
    free(jsonStr);

    if (!SocketMgr::sendMessage(registerCtx.clientFd, json)) {
        return;
    }

    logger->logMsg(DEBUG,
        "register reply sent, code=" + std::to_string(registerCtx.code)
        + ", uid registered", true);
}

void EmbeddedHeartbeatHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) {
        logger->logMsg(ERROR, "embedded heartbeat: empty message", true);
        return;
    }

    EmbeddedHeartbeatBag bag;
    bag.loadFromJsonString(parserCtx.message);

    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "embedded heartbeat: params invalid or incomplete", true);
        return;
    }

    DeviceMgr::getInstance().updateOnlineEmbeddedInfo(parserCtx.senderFd, bag);
}

void FetchDeviceHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) {
        logger->logMsg(ERROR, "fetch devices: empty message", true);
        return ;
    }

    FetchDevicesBag bag;
    bag.loadFromJsonString(parserCtx.message);

    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "fetch devices: params invalid or incomplete", true);
        return ;
    }

    FetchDeviceContext ctx;
    ctx.clientFd = parserCtx.senderFd;
    reply(ctx);
}

void FetchDeviceHandler::reply(const FetchDeviceContext ctx) {
    FetchDevicesAckBag ack;
    DeviceMgr::getInstance().loadAllDeviceInfo(ack.devices);

    char* jsonStr = ack.toJsonString();
    if (!jsonStr) {
        logger->logMsg(ERROR, "fetch devices reply: build json failed", true);
        return;
    }

    std::string json(jsonStr);
    free(jsonStr);

    if (!SocketMgr::sendMessage(ctx.clientFd, json)) {
        return;
    }

    logger->logMsg(DEBUG, "fetch devices reply sent, count="
        + std::to_string(ack.devices.size()), true);
}

void RequestUpdateEmbeddedHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) {
        logger->logMsg(ERROR, "request update embedded: empty message", true);
        return;
    }

    RequestUpdateEmbeddedBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "request update embedded: params invalid", true);
        replyErrorToClient(parserCtx.senderFd, "error");
        return;
    }

    if (!dbMgr->deviceExists(bag.deviceUid)) {
        logger->logMsg(ERROR, "request update embedded: device not found, uid=" + bag.deviceUid, true);
        replyErrorToClient(parserCtx.senderFd, "not_found");
        return;
    }

    int embeddedFd = DeviceMgr::getInstance().getEmbeddedFdByUid(bag.deviceUid);
    if (embeddedFd < 0) {
        logger->logMsg(ERROR, "request update embedded: device offline, uid=" + bag.deviceUid, true);
        replyErrorToClient(parserCtx.senderFd, "offline");
        return;
    }

    RequestUpdateEmbeddedContext ctx;
    ctx.clientFd    = parserCtx.senderFd;
    ctx.embeddedFd  = embeddedFd;
    ctx.deviceUid   = bag.deviceUid;
    ctx.group       = bag.group;
    ctx.name        = bag.name;
    replyToEmbedded(ctx);
}

void RequestUpdateEmbeddedHandler::replyToEmbedded(const RequestUpdateEmbeddedContext& ctx) {
    UpdateEmbeddedInfoBag forward;
    forward.deviceUid = ctx.deviceUid;
    forward.sender    = ctx.clientFd;
    forward.group     = ctx.group;
    forward.name      = ctx.name;

    char* jsonStr = forward.toJsonString();
    if (!jsonStr) {
        logger->logMsg(ERROR, "request update embedded: build forward json failed", true);
        replyErrorToClient(ctx.clientFd, "error");
        return;
    }

    std::string json(jsonStr);
    free(jsonStr);

    if (!SocketMgr::sendMessage(ctx.embeddedFd, json)) {
        replyErrorToClient(ctx.clientFd, "error");
        return;
    }

    logger->logMsg(DEBUG, "update embedded info forwarded, uid=" + ctx.deviceUid, true);
}

void RequestUpdateEmbeddedHandler::replyErrorToClient(int clientFd, const std::string& status) {
    UpdateEmbeddedInfoResultBag result;
    result.status = status;

    char* jsonStr = result.toJsonString();
    if (!jsonStr) {
        logger->logMsg(ERROR, "request update embedded: build error result failed", true);
        return;
    }

    std::string json(jsonStr);
    free(jsonStr);
    SocketMgr::sendMessage(clientFd, json);
}

void UpdateInfoAckHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) {
        logger->logMsg(ERROR, "update info ack: empty message", true);
        return;
    }

    UpdateInfoAckBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "update info ack: params invalid", true);
        return;
    }

    UpdateInfoAckContext ctx;
    ctx.embeddedFd = parserCtx.senderFd;
    ctx.clientFd   = bag.sender;
    ctx.msg        = bag.msg;
    ctx.group      = bag.group;
    ctx.name       = bag.name;
    reply(ctx);
}

void UpdateInfoAckHandler::reply(const UpdateInfoAckContext& ctx) {
    if (ctx.clientFd < 0) {
        logger->logMsg(ERROR, "update info ack: invalid sender fd", true);
        return;
    }

    if (ctx.msg == "ok") {
        OnlineClientInfo* info = DeviceMgr::getInstance().getClient(ctx.embeddedFd);
        if (info && !info->deviceUid.empty()) {
            dbMgr->updateDevice(info->deviceUid, ctx.name, ctx.group);
            DeviceMgr::getInstance().updateOnlineEmbeddedMeta(ctx.embeddedFd, ctx.name, ctx.group);
        }
    }

    UpdateEmbeddedInfoResultBag result;
    result.status = ctx.msg;

    char* jsonStr = result.toJsonString();
    if (!jsonStr) {
        logger->logMsg(ERROR, "update info ack: build result json failed", true);
        return;
    }

    std::string json(jsonStr);
    free(jsonStr);

    if (!SocketMgr::sendMessage(ctx.clientFd, json)) {
        return;
    }

    logger->logMsg(DEBUG, "update embedded info result sent, status=" + ctx.msg, true);
}

void RequestFileListHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) {
        logger->logMsg(ERROR, "request file list: empty message", true);
        return;
    }

    RequestFileListBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "request file list: params invalid", true);
        return;
    }

    RequestFileListContext ctx;
    ctx.clientFd = parserCtx.senderFd;
    reply(ctx);
}

void RequestFileListHandler::reply(const RequestFileListContext& ctx) {
    RequestFileListAckBag ack;
    FileListMgr::getInstance().loadUploadFileList(ack.files);
    ack.count = static_cast<int>(ack.files.size());

    char* jsonStr = ack.toJsonString();
    if (!jsonStr) {
        logger->logMsg(ERROR, "request file list reply: build json failed", true);
        return;
    }

    std::string json(jsonStr);
    free(jsonStr);

    if (!SocketMgr::sendMessage(ctx.clientFd, json)) {
        return;
    }

    logger->logMsg(DEBUG, "request file list reply sent, count="
        + std::to_string(ack.count), true);
}