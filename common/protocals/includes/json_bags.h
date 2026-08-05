#ifndef JSON_BAGS_H
#define JSON_BAGS_H

#include <string>
#include <cstring>
#include <cstdint>
#include <sys/types.h>

extern "C" {
#include <json-c/json.h>
}

struct JsonBagBasic {
    std::string source;
    std::string cmd;
    uint64_t    seq;
    uint32_t    timestamp;
    int         deviceId;
};

struct ParserContext {
    int senderFd;
    JsonBagBasic basic;
    char* message;
    ParserContext(int senderFd, JsonBagBasic basic, char* message) : 
        senderFd(senderFd), basic(basic), message(message) {}
};

class JsonBagHandler {
public: 
    virtual void action(const ParserContext parserCtx) = 0;
};

struct JsonBag {
    std::string     source;
    std::string     cmd; 
    uint64_t        seq;
    uint64_t        timestamp;
    uint32_t        deviceId;
    JsonBag(): source("embedded"), cmd(""), seq(0), timestamp(0), deviceId(0) {}
    JsonBag(const std::string& source): source(source), cmd(""), seq(0), timestamp(0), deviceId(0) {};
    virtual ~JsonBag() {}
    virtual struct json_object* toJsonObject() = 0;
    virtual bool   checkValid() {
        return !source.empty() && !cmd.empty();
    }
    virtual char* toJsonString() = 0;
    virtual void  loadFromJsonString(char* str) = 0;
    struct json_object* packHead() {
        if (!checkValid()) return nullptr;
        struct json_object* root = json_object_new_object();
        json_object_object_add(root, "source", json_object_new_string(source.c_str()));
        json_object_object_add(root, "cmd",    json_object_new_string(cmd.c_str()));
        json_object_object_add(root, "seq",    json_object_new_int64(seq));
        json_object_object_add(root, "timestamp", json_object_new_int64(timestamp));
        json_object_object_add(root, "device_id", json_object_new_int(deviceId));
        return root;
    }
    
};

struct RegisterBag: public JsonBag {
    std::string name;
    std::string group;
    std::string version;
    std::string deviceUid;

    RegisterBag(): JsonBag(), name(""), group(""), version(""), deviceUid("") {
        cmd = "register";
    }
    ~RegisterBag() {}
    bool checkValid() override {
        return !name.empty() && !group.empty() && !version.empty() && !deviceUid.empty();
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct RegisterAckBag: public JsonBag {
    int32_t     code;
    uint32_t    deviceId;
    std::string message;

    RegisterAckBag(): JsonBag("server"), code(0), deviceId(0), message("") {
        cmd = "register_ack";
    }
    ~RegisterAckBag() {}
    bool checkValid() override {
        return source == "server" && cmd == "register_ack";
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

#endif  