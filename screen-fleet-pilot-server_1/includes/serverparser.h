#ifndef SERVERPARSER_H
#define SERVERPARSER_H

#include <cstdint>
#include <string>
#include "jsonparser.h"
#include "databasemgr.h"
#include "devicemgr.h"

class RegisterHandler: public JsonBagHandler {
public:
    struct RegisterContext {
        int32_t     code;
        std::string msg;
        int         clientFd;
    };
    ~RegisterHandler() {}
    void action(const ParserContext parserCtx) override;
    void reply(const RegisterContext registerCtx);
};

class EmbeddedHeartbeatHandler: public JsonBagHandler {
public:
    ~EmbeddedHeartbeatHandler() {}
    void action(const ParserContext parserCtx) override;
};

class ClientHeartbeatHandler: public JsonBagHandler {
public: 
    ~ClientHeartbeatHandler() {}
    void action(const ParserContext parserCtx) override;
};

class FetchDeviceHandler: public JsonBagHandler {
public:
    struct FetchDeviceContext {
        int clientFd;
    };
    ~FetchDeviceHandler() {}
    void action(const ParserContext parserCtx) override;
    void reply(const FetchDeviceContext fetchDeviceCtx);
};

class RequestUpdateEmbeddedHandler: public JsonBagHandler {
public:
    struct RequestUpdateEmbeddedContext {
        int         clientFd;
        int         embeddedFd;
        std::string deviceUid;
        std::string group;
        std::string name;
    };
    ~RequestUpdateEmbeddedHandler() {}
    void action(const ParserContext parserCtx) override;
    void replyToEmbedded(const RequestUpdateEmbeddedContext& ctx);
    void replyErrorToClient(int clientFd, const std::string& status);
};

class UpdateInfoAckHandler: public JsonBagHandler {
public:
    struct UpdateInfoAckContext {
        int         embeddedFd;
        int         clientFd;
        std::string msg;
        std::string group;
        std::string name;
    };
    ~UpdateInfoAckHandler() {}
    void action(const ParserContext parserCtx) override;
    void reply(const UpdateInfoAckContext& ctx);
};

class RequestFileListHandler: public JsonBagHandler {
public:
    struct RequestFileListContext {
        int clientFd;
    };
    ~RequestFileListHandler() {}
    void action(const ParserContext parserCtx) override;
    void reply(const RequestFileListContext& ctx);
};

class MaskDeviceHandler: public JsonBagHandler {
public:
    struct MaskDeviceContext {
        int32_t     code;
        std::string deviceUid;
        std::string message;
        int         clientFd;
    };
    ~MaskDeviceHandler() {}
    void action(const ParserContext parserCtx) override;
    void reply(const MaskDeviceContext ctx);
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
