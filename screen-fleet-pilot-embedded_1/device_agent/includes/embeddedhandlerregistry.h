#ifndef EMBEDDED_HANDLERREGISTRY_H
#define EMBEDDED_HANDLERREGISTRY_H

#include <memory>
#include <string>
#include "json_bags.h"

class EmbeddedHandlerRegistry {
public:
    static std::unique_ptr<JsonBagHandler> create(const std::string& source, const std::string& cmd);
};

#endif
