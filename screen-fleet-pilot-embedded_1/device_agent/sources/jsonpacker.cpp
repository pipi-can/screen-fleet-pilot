#include "../includes/jsonpacker.h"

struct JsonBag {
    std::string     source;
    std::string     cmd; 
    uint64_t        seq;
    uint64_t        timestamp;
    uint32_t        deviceId;
    JsonBag(): source("embedded"), cmd(""), seq(0), timestamp(0), deviceId(0) {}
    virtual ~JsonBag() = 0;
    virtual struct json_object* toJsonObject() = 0;
    virtual bool   checkValid() {
        return !source.empty() && !cmd.empty() && seq > 0 && timestamp > 0 && deviceId > 0;
    }
    virtual char* toJsonString() = 0;
    struct json_object* packHead() {
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
    ~RegisterBag() = default;
    bool checkValid() override {
        return !name.empty() && !group.empty() && !version.empty() && !deviceUid.empty();
    }
    struct json_object* toJsonObject() override {
        if (!checkValid()) return nullptr;
        
        struct json_object* root = packHead();

        struct json_object* paramsObj = json_object_new_object();
        json_object_object_add(paramsObj, "name",       json_object_new_string(name.c_str()));
        json_object_object_add(paramsObj, "group",      json_object_new_string(group.c_str()));
        json_object_object_add(paramsObj, "version",    json_object_new_string(version.c_str()));
        json_object_object_add(paramsObj, "device_uid", json_object_new_string(deviceUid.c_str()));

        json_object_object_add(root, "params", paramsObj);
        return root;
    }
    char* toJsonString() override {
        struct json_object* root = toJsonObject();
        const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
        char* result = strdup(jsonStr);
        json_object_put(root);
        return result;
    }
};