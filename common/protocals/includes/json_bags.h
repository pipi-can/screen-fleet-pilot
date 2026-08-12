#ifndef JSON_BAGS_H
#define JSON_BAGS_H

#include <json-c/json_object.h>
#include <string>
#include <cstring>
#include <cstdint>
#include <vector>
#include <sys/types.h>

extern "C" {
#include <json-c/json.h>
}

struct JsonBagBasic {
    std::string source;
    std::string cmd;
    uint64_t    seq;
    uint32_t    timestamp;
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
    virtual ~JsonBagHandler() {}
    virtual void action(const ParserContext parserCtx) = 0;
};

struct JsonBag {
    std::string     source;
    std::string     cmd; 
    uint64_t        seq;
    uint64_t        timestamp;
    JsonBag(): source("embedded"), cmd(""), seq(0), timestamp(0) {}
    JsonBag(const std::string& source): source(source), cmd(""), seq(0), timestamp(0) {}
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
    std::string message;

    RegisterAckBag(): JsonBag("server"), code(0), message("") {
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

struct EmbeddedHeartbeatBag: public JsonBag {
    std::string cpuTemp;
    int         memUsage;
    int         diskFreeMb;

    EmbeddedHeartbeatBag(): JsonBag(), cpuTemp("-1"), memUsage(-1), diskFreeMb(-1) {
        cmd = "heartbeat";
    }
    ~EmbeddedHeartbeatBag() {}
    bool checkValid() override {
        return source == "embedded" && cmd == "heartbeat";
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct ClientHeartbeatBag: public JsonBag {
    ClientHeartbeatBag(): JsonBag("client") {
        cmd = "heartbeat";
    }
    ~ClientHeartbeatBag(){}
    bool checkValid() override {
        return source == "client" && cmd == "heartbeat";
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct FetchDevicesBag: public JsonBag {
    FetchDevicesBag(): JsonBag("client") {
        cmd = "fetch_devices";
    }
    ~FetchDevicesBag() {}
    bool checkValid() override {
        return source == "client" && cmd == "fetch_devices";
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct DeviceEntry {
    std::string deviceUid;
    std::string name;
    std::string group;
    std::string version;
    std::string temperature;
    int memUsage;
    int diskFreeMb;
    uint64_t timestamp;
    bool online;

    DeviceEntry() : deviceUid(""), name(""), group(""), version(""),
                    temperature(""), memUsage(-1), diskFreeMb(-1), timestamp(0), online(false) {}
    struct json_object *toJsonObject() const {
        struct json_object *root = json_object_new_object();
        json_object_object_add(root, "device_uid", json_object_new_string(deviceUid.c_str()));
        json_object_object_add(root, "name", json_object_new_string(name.c_str()));
        json_object_object_add(root, "group", json_object_new_string(group.c_str()));
        json_object_object_add(root, "version", json_object_new_string(version.c_str()));
        json_object_object_add(root, "temperature", json_object_new_string(temperature.c_str()));
        json_object_object_add(root, "mem_usage", json_object_new_int(memUsage));
        json_object_object_add(root, "disk_free_mb", json_object_new_int(diskFreeMb));
        json_object_object_add(root, "timestamp", json_object_new_int64(timestamp));
        json_object_object_add(root, "online", json_object_new_boolean(online));
        return root;
    }
};

struct FetchDevicesAckBag: public JsonBag {
    std::vector<DeviceEntry> devices;

    FetchDevicesAckBag(): JsonBag("server") {
        cmd = "fetch_devices_ack";
    }
    ~FetchDevicesAckBag() {}
    bool checkValid() override {
        return source == "server" && cmd == "fetch_devices_ack";
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct RequestUpdateEmbeddedBag: public JsonBag {
    std::string deviceUid;
    std::string group;
    std::string name;

    RequestUpdateEmbeddedBag(): JsonBag("client"), deviceUid(""), group(""), name("") {
        cmd = "request_update_embedded";
    }
    ~RequestUpdateEmbeddedBag() {}
    bool checkValid() override {
        return source == "client" && cmd == "request_update_embedded"
            && !deviceUid.empty() && !group.empty() && !name.empty();
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct UpdateEmbeddedInfoBag: public JsonBag {
    std::string deviceUid;
    int         sender;
    std::string group;
    std::string name;

    UpdateEmbeddedInfoBag(): JsonBag("server"), deviceUid(""), sender(-1), group(""), name("") {
        cmd = "update_embedded_info";
    }
    ~UpdateEmbeddedInfoBag() {}
    bool checkValid() override {
        return source == "server" && cmd == "update_embedded_info"
            && !deviceUid.empty() && sender >= 0 && !group.empty() && !name.empty();
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct UpdateInfoAckBag: public JsonBag {
    std::string msg;
    int         sender;
    std::string group;
    std::string name;

    UpdateInfoAckBag(): JsonBag(), msg(""), sender(-1), group(""), name("") {
        cmd = "update_info_ack";
    }
    ~UpdateInfoAckBag() {}
    bool checkValid() override {
        return source == "embedded" && cmd == "update_info_ack"
            && !msg.empty() && sender >= 0;
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct UpdateEmbeddedInfoResultBag: public JsonBag {
    std::string status;

    UpdateEmbeddedInfoResultBag(): JsonBag("server"), status("") {
        cmd = "update_embedded_info_result";
    }
    ~UpdateEmbeddedInfoResultBag() {}
    bool checkValid() override {
        return source == "server" && cmd == "update_embedded_info_result" && !status.empty();
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct FileListEntry {
    std::string path;
    std::string name;
    int64_t     size;

    FileListEntry(): path(""), name(""), size(0) {}
    struct json_object* toJsonObject() const {
        struct json_object* root = json_object_new_object();
        json_object_object_add(root, "path", json_object_new_string(path.c_str()));
        json_object_object_add(root, "name", json_object_new_string(name.c_str()));
        json_object_object_add(root, "size", json_object_new_int64(size));
        return root;
    }
};

struct RequestFileListBag: public JsonBag {
    RequestFileListBag(): JsonBag("client") {
        cmd = "request_file_list";
    }
    ~RequestFileListBag() {}
    bool checkValid() override {
        return source == "client" && cmd == "request_file_list";
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct RequestFileListAckBag: public JsonBag {
    int count;
    std::vector<FileListEntry> files;

    RequestFileListAckBag(): JsonBag("server"), count(0) {
        cmd = "request_filelist_ack";
    }
    ~RequestFileListAckBag() {}
    bool checkValid() override {
        return source == "server" && cmd == "request_filelist_ack";
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct MaskDeviceBag: public JsonBag {
    std::string deviceUid;

    MaskDeviceBag(): JsonBag("client"), deviceUid("") {
        cmd = "mask_device";
    }
    ~MaskDeviceBag() {}
    bool checkValid() override {
        return source == "client" && cmd == "mask_device" && !deviceUid.empty();
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct MaskDeviceAckBag: public JsonBag {
    int32_t     code;
    std::string deviceUid;
    std::string message;

    MaskDeviceAckBag(): JsonBag("server"), code(0), deviceUid(""), message("") {
        cmd = "mask_device_ack";
    }
    ~MaskDeviceAckBag() {}
    bool checkValid() override {
        return source == "server" && cmd == "mask_device_ack";
    }
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};
#endif  