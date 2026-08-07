#include "../includes/embeddedparser.h"

static LogMgr* logger = &LogMgr::getInstance();

void EmbeddedParser::parseMessage(ParserContext parserCtx) {
    JsonBagBasic basic = parserCtx.basic;
    JsonBagHandler* handler = nullptr;
    if (basic.source == "server") {
        if (basic.cmd == "register_ack") {
            handler = new RegisterAckHandler();
            handler->action(parserCtx);
            delete handler;
        } else if (basic.cmd == "update_embedded_info") {
            handler = new UpdateEmbeddedInfoHandler();
            handler->action(parserCtx);
            delete handler;
        }
    }
}

void RegisterAckHandler::action(const ParserContext parserCtx) {
    if (!parserCtx.message) {
        logger->logMsg(ERROR, "register: empty message", true);
        return;
    }

    static Client* client = &Client::getInstance();
    RegisterAckBag bag;
    bag.loadFromJsonString(parserCtx.message);

    if (!bag.checkValid()) {
        logger->logMsg(ERROR, "register: params invalid or incomplete", true);
        return;
    }

    if (bag.code == 0 || bag.code == 1) {
        client->setRegistered(true);
        int ret = client->initHeartbeatTimer();
        if (ret == -2) {
            logger->logMsg(ERROR, "init heartbeat timer failed", true);
        } else {
            logger->logMsg(DEBUG, "init heartbeat timer success", true);
            client->startHeartbeatTimer();
            client->sendHeartbeatBagToServer();
        }
    } else {
        Client::getInstance().setRegistered(false);
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
        logger->logMsg(ERROR, "update embedded info: uid mismatch", true);
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
    } else {
        logger->logMsg(DEBUG, "update embedded info ack sent", true);
    }
    free(jsonStr);
}
