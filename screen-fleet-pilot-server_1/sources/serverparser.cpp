#include "../includes/serverparser.h"
#include "logmgr.h"
#include "socketmgr.h"
#include <cstdlib>
#include <string>

static LogMgr* logger = &LogMgr::getInstance();
static DatabaseMgr* dbMgr = &DatabaseMgr::getInstance();

int RegisterHandler::distributeId = 0;

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
        }
    }
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
        ctx.code     = 1;
        ctx.deviceId = parserCtx.basic.deviceId;
        ctx.msg      = "repeated";
        reply(ctx);
        return ;
    }
    if (!dbMgr->insertDevice(bag.deviceUid, bag.name, bag.group, bag.source)) {
        ctx.code     = -1;
        ctx.deviceId = 0;
        ctx.msg      = "error";
        reply(ctx);
        logger->logMsg(ERROR, "register: device register failed, uid: " + bag.deviceUid, true);
    } else {
        ctx.code     = 0;
        ctx.deviceId = ++RegisterHandler::distributeId;
        ctx.msg      = "ok";
        reply(ctx);
        logger->logMsg(DEBUG, "register: device register success, uid: " + bag.deviceUid, true);
    }
}

void RegisterHandler::reply(const RegisterContext registerCtx) {
    RegisterAckBag ack;
    ack.code     = registerCtx.code;
    ack.deviceId = registerCtx.deviceId;
    ack.message  = registerCtx.msg;

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
        + ", device_id=" + std::to_string(registerCtx.deviceId), true);
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