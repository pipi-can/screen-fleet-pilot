#ifndef EMBEDDEDPARSER_H
#define EMBEDDEDPARSER_H

#include "jsonparser.h"
#include "client.h"
class RegisterAckHandler: public JsonBagHandler {
public: 
    ~RegisterAckHandler() {}
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
