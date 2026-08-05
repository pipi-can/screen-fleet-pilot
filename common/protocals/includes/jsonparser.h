#ifndef JSONPARSER_H
#define JSONPARSER_H

#include "json_bags.h"

class JsonParser {
public:
    JsonParser(const JsonParser& other) = delete;
    JsonParser& operator=(const JsonParser& other) = delete;
    virtual ~JsonParser() = default;

    static JsonBagBasic parseBasic(char* jsonStr);
    virtual void parseMessage(ParserContext parserCtx) = 0;

protected:
    JsonParser() = default;
};

#endif
