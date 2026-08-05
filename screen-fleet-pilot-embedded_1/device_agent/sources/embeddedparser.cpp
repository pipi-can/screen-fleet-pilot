#include "../includes/embeddedparser.h"

void EmbeddedParser::parseMessage(ParserContext& parserCtx) {
    JsonBagBasic& basic = parserCtx.basic;
    JsonBagHandler* handler = nullptr;
    if (basic.source == "server") {
        if (basic.cmd == "register_ack") {
            handler = new RegisterAckHandler();
            handler->action();
            delete handler;
        }
    }
}

void RegisterAckHandler::action(const ParserContext& parserCtx) override {
    if (!parserCtx.message) return ;
    
}