#include "../includes/jsonparser.h"

JsonBagBasic JsonParser::parseBasic(char* jsonStr) {
    JsonBagBasic basic{};

    if (!jsonStr || jsonStr[0] == '\0') {
        return basic;
    }

    struct json_object* root = json_tokener_parse(jsonStr);
    if (!root) return basic;

    struct json_object* obj = nullptr;

    if (json_object_object_get_ex(root, "source", &obj)
        && json_object_is_type(obj, json_type_string)) {
        basic.source = json_object_get_string(obj);
    }

    if (json_object_object_get_ex(root, "cmd", &obj)
        && json_object_is_type(obj, json_type_string)) {
        basic.cmd = json_object_get_string(obj);
    }

    if (json_object_object_get_ex(root, "seq", &obj)) {
        basic.seq = json_object_get_int64(obj);
    }

    if (json_object_object_get_ex(root, "timestamp", &obj)) {
        basic.timestamp = static_cast<uint32_t>(json_object_get_int64(obj));
    }

    if (json_object_object_get_ex(root, "device_id", &obj)) {
        basic.deviceId = json_object_get_int(obj);
    }

    json_object_put(root);
    return basic;
}
