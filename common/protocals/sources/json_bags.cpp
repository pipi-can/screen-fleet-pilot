#include "../includes/json_bags.h"
#include <cstdlib>

static bool readStringField(struct json_object* parent, const char* key, std::string& out) {
    struct json_object* obj = nullptr;
    if (!json_object_object_get_ex(parent, key, &obj)
        || !json_object_is_type(obj, json_type_string)) {
        return false;
    }
    out = json_object_get_string(obj);
    return true;
}

struct json_object* RegisterBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }

    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "name",       json_object_new_string(name.c_str()));
    json_object_object_add(paramsObj, "group",      json_object_new_string(group.c_str()));
    json_object_object_add(paramsObj, "version",    json_object_new_string(version.c_str()));
    json_object_object_add(paramsObj, "device_uid", json_object_new_string(deviceUid.c_str()));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* RegisterBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) {
        return nullptr;
    }
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

/**
 * @brief: 从一行 JSON 解析 register 包，填充 JsonBag 公共字段与 params
 * @param: str，以 \\0 结尾的 JSON 字符串，来源是 recv 切帧后的 buffer
 */
void RegisterBag::loadFromJsonString(char* str) {
    if (!str) {
        return;
    }

    struct json_object* root = json_tokener_parse(str);
    if (!root) {
        return;
    }

    struct json_object* obj = nullptr;

    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);

    if (json_object_object_get_ex(root, "seq", &obj)) {
        seq = json_object_get_int64(obj);
    }
    if (json_object_object_get_ex(root, "timestamp", &obj)) {
        timestamp = json_object_get_int64(obj);
    }

    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringField(paramsObj, "name", name);
        readStringField(paramsObj, "group", group);
        readStringField(paramsObj, "version", version);
        readStringField(paramsObj, "device_uid", deviceUid);
    }

    json_object_put(root);
}

struct json_object* RegisterAckBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }

    struct json_object* root = packHead();
    if (!root) {
        return nullptr;
    }

    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "code", json_object_new_int(code));
    json_object_object_add(paramsObj, "msg",  json_object_new_string(message.c_str()));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* RegisterAckBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) {
        return nullptr;
    }
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void RegisterAckBag::loadFromJsonString(char* str) {
    if (!str) {
        return;
    }

    struct json_object* root = json_tokener_parse(str);
    if (!root) {
        return;
    }

    struct json_object* obj = nullptr;

    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);

    if (json_object_object_get_ex(root, "seq", &obj)) {
        seq = json_object_get_int64(obj);
    }
    if (json_object_object_get_ex(root, "timestamp", &obj)) {
        timestamp = json_object_get_int64(obj);
    }

    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        if (json_object_object_get_ex(paramsObj, "code", &obj)) {
            code = json_object_get_int(obj);
        }
        readStringField(paramsObj, "msg", message);
    }

    json_object_put(root);
}

struct json_object* EmbeddedHeartbeatBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }

    struct json_object* root = packHead();
    if (!root) {
        return nullptr;
    }

    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "cpu_temp",        json_object_new_string(cpuTemp.c_str()));
    json_object_object_add(paramsObj, "mem_usage",       json_object_new_int(memUsage));
    json_object_object_add(paramsObj, "disk_free_mb",    json_object_new_int(diskFreeMb));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* EmbeddedHeartbeatBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) {
        return nullptr;
    }
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void EmbeddedHeartbeatBag::loadFromJsonString(char* str) {
    if (!str) {
        return;
    }

    struct json_object* root = json_tokener_parse(str);
    if (!root) {
        return;
    }

    struct json_object* obj = nullptr;

    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);

    if (json_object_object_get_ex(root, "seq", &obj)) {
        seq = json_object_get_int64(obj);
    }
    if (json_object_object_get_ex(root, "timestamp", &obj)) {
        timestamp = json_object_get_int64(obj);
    }

    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringField(paramsObj, "cpu_temp", cpuTemp);
        if (json_object_object_get_ex(paramsObj, "mem_usage", &obj)) {
            memUsage = json_object_get_int(obj);
        }
        if (json_object_object_get_ex(paramsObj, "disk_free_mb", &obj)) {
            diskFreeMb = json_object_get_int(obj);
        }
    }

    json_object_put(root);
}

struct json_object* FetchDevicesBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }

    struct json_object* root = packHead();
    if (!root) {
        return nullptr;
    }

    json_object_object_add(root, "params", json_object_new_object());
    return root;
}

char* FetchDevicesBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) {
        return nullptr;
    }
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void FetchDevicesBag::loadFromJsonString(char* str) {
    if (!str) {
        return;
    }

    struct json_object* root = json_tokener_parse(str);
    if (!root) {
        return;
    }

    struct json_object* obj = nullptr;

    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);

    if (json_object_object_get_ex(root, "seq", &obj)) {
        seq = json_object_get_int64(obj);
    }
    if (json_object_object_get_ex(root, "timestamp", &obj)) {
        timestamp = json_object_get_int64(obj);
    }

    json_object_put(root);
}

struct json_object* FetchDevicesAckBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }

    struct json_object* root = packHead();
    if (!root) {
        return nullptr;
    }

    struct json_object* paramsObj = json_object_new_object();
    struct json_object* devicesArr = json_object_new_array();
    for (const auto& device : devices) {
        json_object_array_add(devicesArr, device.toJsonObject());
    }
    json_object_object_add(paramsObj, "devices", devicesArr);
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* FetchDevicesAckBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) {
        return nullptr;
    }
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void FetchDevicesAckBag::loadFromJsonString(char* str) {
    if (!str) {
        return;
    }

    struct json_object* root = json_tokener_parse(str);
    if (!root) {
        return;
    }

    struct json_object* obj = nullptr;

    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);

    if (json_object_object_get_ex(root, "seq", &obj)) {
        seq = json_object_get_int64(obj);
    }
    if (json_object_object_get_ex(root, "timestamp", &obj)) {
        timestamp = json_object_get_int64(obj);
    }

    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        struct json_object* devicesArr = nullptr;
        if (json_object_object_get_ex(paramsObj, "devices", &devicesArr)
            && json_object_is_type(devicesArr, json_type_array)) {
            int len = json_object_array_length(devicesArr);
            devices.clear();
            for (int i = 0; i < len; ++i) {
                struct json_object* item = json_object_array_get_idx(devicesArr, i);
                if (!json_object_is_type(item, json_type_object)) {
                    continue;
                }
                DeviceEntry entry;
                readStringField(item, "device_uid", entry.deviceUid);
                readStringField(item, "name", entry.name);
                readStringField(item, "group", entry.group);
                readStringField(item, "version", entry.version);
                readStringField(item, "temperature", entry.temperature);
                if (json_object_object_get_ex(item, "mem_usage", &obj)) {
                    entry.memUsage = json_object_get_int(obj);
                }
                if (json_object_object_get_ex(item, "disk_free_mb", &obj)) {
                    entry.diskFreeMb = json_object_get_int(obj);
                }
                if (json_object_object_get_ex(item, "timestamp", &obj)) {
                    entry.timestamp = json_object_get_int64(obj);
                }
                if (json_object_object_get_ex(item, "online", &obj)) {
                    entry.online = json_object_get_boolean(obj);
                }
                devices.push_back(entry);
            }
        }
    }

    json_object_put(root);
}

struct json_object* RequestUpdateEmbeddedBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "device_uid", json_object_new_string(deviceUid.c_str()));
    json_object_object_add(paramsObj, "group",      json_object_new_string(group.c_str()));
    json_object_object_add(paramsObj, "name",       json_object_new_string(name.c_str()));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* RequestUpdateEmbeddedBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) return nullptr;
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void RequestUpdateEmbeddedBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    struct json_object* obj = nullptr;
    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);
    if (json_object_object_get_ex(root, "seq", &obj)) seq = json_object_get_int64(obj);
    if (json_object_object_get_ex(root, "timestamp", &obj)) timestamp = json_object_get_int64(obj);
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringField(paramsObj, "device_uid", deviceUid);
        readStringField(paramsObj, "group", group);
        readStringField(paramsObj, "name", name);
    }
    json_object_put(root);
}

struct json_object* UpdateEmbeddedInfoBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "device_uid", json_object_new_string(deviceUid.c_str()));
    json_object_object_add(paramsObj, "sender",     json_object_new_int(sender));
    json_object_object_add(paramsObj, "group",      json_object_new_string(group.c_str()));
    json_object_object_add(paramsObj, "name",       json_object_new_string(name.c_str()));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* UpdateEmbeddedInfoBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) return nullptr;
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void UpdateEmbeddedInfoBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    struct json_object* obj = nullptr;
    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);
    if (json_object_object_get_ex(root, "seq", &obj)) seq = json_object_get_int64(obj);
    if (json_object_object_get_ex(root, "timestamp", &obj)) timestamp = json_object_get_int64(obj);
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringField(paramsObj, "device_uid", deviceUid);
        if (json_object_object_get_ex(paramsObj, "sender", &obj)) sender = json_object_get_int(obj);
        readStringField(paramsObj, "group", group);
        readStringField(paramsObj, "name", name);
    }
    json_object_put(root);
}

struct json_object* UpdateInfoAckBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "msg",    json_object_new_string(msg.c_str()));
    json_object_object_add(paramsObj, "sender", json_object_new_int(sender));
    json_object_object_add(paramsObj, "group",  json_object_new_string(group.c_str()));
    json_object_object_add(paramsObj, "name",   json_object_new_string(name.c_str()));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* UpdateInfoAckBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) return nullptr;
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void UpdateInfoAckBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    struct json_object* obj = nullptr;
    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);
    if (json_object_object_get_ex(root, "seq", &obj)) seq = json_object_get_int64(obj);
    if (json_object_object_get_ex(root, "timestamp", &obj)) timestamp = json_object_get_int64(obj);
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringField(paramsObj, "msg", msg);
        if (json_object_object_get_ex(paramsObj, "sender", &obj)) sender = json_object_get_int(obj);
        readStringField(paramsObj, "group", group);
        readStringField(paramsObj, "name", name);
    }
    json_object_put(root);
}

struct json_object* UpdateEmbeddedInfoResultBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "status", json_object_new_string(status.c_str()));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* UpdateEmbeddedInfoResultBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) return nullptr;
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void UpdateEmbeddedInfoResultBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    struct json_object* obj = nullptr;
    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);
    if (json_object_object_get_ex(root, "seq", &obj)) seq = json_object_get_int64(obj);
    if (json_object_object_get_ex(root, "timestamp", &obj)) timestamp = json_object_get_int64(obj);
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringField(paramsObj, "status", status);
    }
    json_object_put(root);
}

struct json_object* RequestFileListBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }
    struct json_object* root = packHead();
    json_object_object_add(root, "params", json_object_new_object());
    return root;
}

char* RequestFileListBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) return nullptr;
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void RequestFileListBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    struct json_object* obj = nullptr;
    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);
    if (json_object_object_get_ex(root, "seq", &obj)) seq = json_object_get_int64(obj);
    if (json_object_object_get_ex(root, "timestamp", &obj)) timestamp = json_object_get_int64(obj);
    json_object_put(root);
}

struct json_object* RequestFileListAckBag::toJsonObject() {
    if (!checkValid()) {
        return nullptr;
    }
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "count", json_object_new_int(count));
    struct json_object* filesArr = json_object_new_array();
    for (const auto& file : files) {
        json_object_array_add(filesArr, file.toJsonObject());
    }
    json_object_object_add(paramsObj, "files", filesArr);
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* RequestFileListAckBag::toJsonString() {
    struct json_object* root = toJsonObject();
    if (!root) return nullptr;
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void RequestFileListAckBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    struct json_object* obj = nullptr;
    readStringField(root, "source", source);
    readStringField(root, "cmd", cmd);
    if (json_object_object_get_ex(root, "seq", &obj)) seq = json_object_get_int64(obj);
    if (json_object_object_get_ex(root, "timestamp", &obj)) timestamp = json_object_get_int64(obj);
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        if (json_object_object_get_ex(paramsObj, "count", &obj)) {
            count = json_object_get_int(obj);
        }
        struct json_object* filesArr = nullptr;
        if (json_object_object_get_ex(paramsObj, "files", &filesArr)
            && json_object_is_type(filesArr, json_type_array)) {
            int len = json_object_array_length(filesArr);
            files.clear();
            for (int i = 0; i < len; ++i) {
                struct json_object* item = json_object_array_get_idx(filesArr, i);
                if (!json_object_is_type(item, json_type_object)) {
                    continue;
                }
                FileListEntry entry;
                readStringField(item, "path", entry.path);
                readStringField(item, "name", entry.name);
                if (json_object_object_get_ex(item, "size", &obj)) {
                    entry.size = json_object_get_int64(obj);
                }
                files.push_back(entry);
            }
        }
    }
    json_object_put(root);
}

