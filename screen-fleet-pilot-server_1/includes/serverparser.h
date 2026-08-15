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

class ClientRequestScreenshotHandler: public JsonBagHandler {
public:
    ~ClientRequestScreenshotHandler() {}
    void action(const ParserContext parserCtx) override;
    void replyToEmbedded(int clientFd, int embeddedFd);
};

class EmbeddedScreenshotDataHandler: public JsonBagHandler {
public:
    ~EmbeddedScreenshotDataHandler() {}
    void action(const ParserContext parserCtx) override;
    void replyToClient(int clientFd, const std::string& path);
};

class RequestPushContentHandler: public JsonBagHandler {
public:
    ~RequestPushContentHandler() {}
    void action(const ParserContext parserCtx) override;
};

class RequestSchedulePushHandler: public JsonBagHandler {
public:
    ~RequestSchedulePushHandler() {}
    void action(const ParserContext parserCtx) override;
};

class RequestFirmwareListHandler: public JsonBagHandler {
public:
    ~RequestFirmwareListHandler() {}
    void action(const ParserContext parserCtx) override;
};

class RequestOtaUpdateHandler: public JsonBagHandler {
public:
    ~RequestOtaUpdateHandler() {}
    void action(const ParserContext parserCtx) override;
};

class EmbeddedOtaUpdateAckHandler: public JsonBagHandler {
public:
    ~EmbeddedOtaUpdateAckHandler() {}
    void action(const ParserContext parserCtx) override;
};

class RequestCheckFirmwareHandler: public JsonBagHandler {
public:
    ~RequestCheckFirmwareHandler() {}
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
