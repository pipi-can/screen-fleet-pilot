#ifndef HANDLERREGISTRY_H
#define HANDLERREGISTRY_H

#include <memory>
#include <string>
#include "json_bags.h"

class HandlerRegistry {
public:
    static std::unique_ptr<JsonBagHandler> create(const std::string& source, const std::string& cmd);
};

#endif
