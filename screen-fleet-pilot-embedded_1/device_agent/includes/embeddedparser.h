#ifndef EMBEDDEDPARSER_H
#define EMBEDDEDPARSER_H

#include "jsonparser.h"
#include "client.h"

class RegisterAckHandler: public JsonBagHandler {
public:
    ~RegisterAckHandler() {}
    void action(const ParserContext parserCtx) override;
};

class UpdateEmbeddedInfoHandler: public JsonBagHandler {
public:
    struct UpdateEmbeddedInfoContext {
        int         sender;
        std::string group;
        std::string name;
        std::string msg;
    };
    ~UpdateEmbeddedInfoHandler() {}
    void action(const ParserContext parserCtx) override;
    void reply(const UpdateEmbeddedInfoContext& ctx);
};

class PushResourcesHandler: public JsonBagHandler {
public:
    ~PushResourcesHandler() {}
    void action(const ParserContext parserCtx) override;
};

class PushScheduleHandler: public JsonBagHandler {
public:
    ~PushScheduleHandler() {}
    void action(const ParserContext parserCtx) override;
};

class ScreenshotRequestHandler: public JsonBagHandler {
public:
    ~ScreenshotRequestHandler() {}
    void action(const ParserContext parserCtx) override;
};

class OtaUpdateHandler: public JsonBagHandler {
public:
    ~OtaUpdateHandler() {}
    void action(const ParserContext parserCtx) override;
};

class EmbeddedParser: public JsonParser {
public:
    EmbeddedParser(const EmbeddedParser& other) = delete;
    EmbeddedParser& operator=(const EmbeddedParser& other) = delete;

    static EmbeddedParser& getInstance() {
        static EmbeddedParser instance;
        return instance;
    }

    void parseMessage(ParserContext parserCtx) override;

private:
    EmbeddedParser() = default;
};

#endif
