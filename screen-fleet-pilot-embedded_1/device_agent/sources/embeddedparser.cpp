#include "../includes/embeddedparser.h"
#include "../includes/embeddedhandlerregistry.h"
#include "../includes/contentmgr.h"
#include "logmgr.h"
#include <cstdlib>
#include <memory>

static LogMgr* logger = &LogMgr::getInstance();

void EmbeddedParser::parseMessage(ParserContext parserCtx) {
    JsonBagBasic& basic = parserCtx.basic;
    std::unique_ptr<JsonBagHandler> handler = EmbeddedHandlerRegistry::create(basic.source, basic.cmd);
    if (!handler) {
        logger->logMsg(ERROR, "unknown message: " + basic.source + "/" + basic.cmd, true);
        return;
    }
    handler->action(parserCtx);
}

void RegisterAckHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) {
        logger->logMsg(ERROR, "register: empty message", true);
        return;
    }

    Client* client = &Client::getInstance();
    RegisterAckBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "register: params invalid or incomplete", true);
        return;
    }

    if (bag.code == 0 || bag.code == 1) {
        client->setRegistered(true);
        if (client->initHeartbeatTimer() == 0) {
            client->startHeartbeatTimer();
            client->sendHeartbeatBagToServer();
        }
    } else {
        client->setRegistered(false);
    }
}

void UpdateEmbeddedInfoHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) {
        logger->logMsg(ERROR, "update embedded info: empty message", true);
        return;
    }

    UpdateEmbeddedInfoBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "update embedded info: params invalid", true);
        return;
    }

    Client* client = &Client::getInstance();
    if (bag.deviceUid != client->getDeviceUid()) {
        UpdateEmbeddedInfoContext ctx;
        ctx.sender = bag.sender;
        ctx.group  = client->getGroup();
        ctx.name   = client->getName();
        ctx.msg    = "error";
        reply(ctx);
        return;
    }

    client->setName(bag.name);
    client->setGroup(bag.group);

    UpdateEmbeddedInfoContext ctx;
    ctx.sender = bag.sender;
    ctx.group  = client->getGroup();
    ctx.name   = client->getName();
    ctx.msg    = "ok";
    reply(ctx);
}

void UpdateEmbeddedInfoHandler::reply(const UpdateEmbeddedInfoContext& ctx) {
    UpdateInfoAckBag ack;
    ack.msg    = ctx.msg;
    ack.sender = ctx.sender;
    ack.group  = ctx.group;
    ack.name   = ctx.name;

    char* jsonStr = ack.toJsonString();
    if (!jsonStr) {
        logger->logMsg(ERROR, "update embedded info: build ack failed", true);
        return;
    }
    if (!Client::getInstance().sendMessageToServer(jsonStr)) {
        logger->logMsg(ERROR, "update embedded info: send ack failed", true);
    }
    free(jsonStr);
}

void PushResourcesHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) return;
    PushResourcesToDownloadBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "push resources: invalid bag", true);
        return;
    }
    ContentMgr::getInstance().handlePushResources(bag);
}

void PushScheduleHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) return;
    PushSchedulePlaylistBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "push schedule: invalid bag", true);
        return;
    }
    ContentMgr::getInstance().handlePushSchedule(bag);
}

void ScreenshotRequestHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) return;
    ServerRequestScreenshotBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "screenshot request: invalid bag", true);
        return;
    }
    ContentMgr::getInstance().handleScreenshotRequest(bag.requestClientFd);
}

void OtaUpdateHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) return;
    OtaUpdateBag bag;
    bag.loadFromJsonString(parserCtx.message);
    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "ota update: invalid bag", true);
        return;
    }
    ContentMgr::getInstance().handleOtaUpdate(bag);
}
