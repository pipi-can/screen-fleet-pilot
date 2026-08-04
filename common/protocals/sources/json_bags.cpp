#include "../includes/json_bags.h"

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
    if (json_object_object_get_ex(root, "device_id", &obj)) {
        deviceId = json_object_get_int(obj);
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