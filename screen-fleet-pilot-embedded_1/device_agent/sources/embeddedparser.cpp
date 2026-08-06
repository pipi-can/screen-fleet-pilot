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
        }
    } else {
        Client::getInstance().setRegistered(false);
    }
}