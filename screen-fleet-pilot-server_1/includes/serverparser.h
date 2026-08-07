#ifndef SERVERPARSER_H
#define SERVERPARSER_H

#include <cstdint>
#include <string>
#include "jsonparser.h"
#include "databasemgr.h"
#include "devicemgr.h"
#include "logmgr.h"

static LogMgr* logger = &LogMgr::getInstance();

class RegisterHandler: public JsonBagHandler {
public:
    struct RegisterContext {
        int32_t     code;
        uint32_t    deviceId;
        std::string msg;
        int         clientFd;
    };
    ~RegisterHandler() {}
    void action(const ParserContext parserCtx) override;
    void reply(const RegisterContext registerCtx);

    static int distributeId;
};

class EmbeddedHeartbeatHandler: public JsonBagHandler {
public: 
    ~EmbeddedHeartbeatHandler() {}
    void action(const ParserContext parserCtx) override;
};

class ServerParser: public JsonParser {
public:
    ServerParser(const ServerParser& other) = delete;
    ServerParser& operator=(const ServerParser& other) = delete;

    static ServerParser& getInstance() {
        static ServerParser instance;
        return instance;
    }

    void parseMessage(ParserContext parserCtx) override;

private:
    ServerParser() = default;
};

#endif
