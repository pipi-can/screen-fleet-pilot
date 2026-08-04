#ifndef JSONPACKER_H
#define JSONPACKER_H

extern "C" {
#include <json-c/json.h>
}
#include <memory>

extern struct RegisterBag;

enum JsonPackerType {
    REGISTER = 0, 
    HEARTBEAT,
};

class JsonPacker {
public: 
    JsonPacker() = default;
    virtual ~JsonPacker() = 0;

};

#endif 