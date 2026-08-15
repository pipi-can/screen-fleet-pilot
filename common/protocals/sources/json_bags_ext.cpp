#include "../includes/json_bags_ext.h"
#include <cstdlib>

namespace {

bool readStringField(struct json_object* parent, const char* key, std::string& out) {
    struct json_object* obj = nullptr;
    if (!json_object_object_get_ex(parent, key, &obj)
        || !json_object_is_type(obj, json_type_string)) {
        return false;
    }
    out = json_object_get_string(obj);
    return true;
}

void readStringArray(struct json_object* parent, const char* key, std::vector<std::string>& out) {
    struct json_object* arr = nullptr;
    if (!json_object_object_get_ex(parent, key, &arr)
        || !json_object_is_type(arr, json_type_array)) {
        return;
    }
    int len = json_object_array_length(arr);
    for (int i = 0; i < len; i++) {
        struct json_object* item = json_object_array_get_idx(arr, i);
        if (item && json_object_is_type(item, json_type_string)) {
            const char* s = json_object_get_string(item);
            if (s && s[0]) {
                out.push_back(s);
            }
        }
    }
}

struct json_object* stringArrayToJson(const std::vector<std::string>& items) {
    struct json_object* arr = json_object_new_array();
    for (const std::string& s : items) {
        json_object_array_add(arr, json_object_new_string(s.c_str()));
    }
    return arr;
}

char* bagToString(JsonBag& bag) {
    struct json_object* root = bag.toJsonObject();
    if (!root) {
        return nullptr;
    }
    const char* jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = strdup(jsonStr);
    json_object_put(root);
    return result;
}

void loadBagHead(struct json_object* root, JsonBag& bag) {
    struct json_object* obj = nullptr;
    readStringField(root, "source", bag.source);
    readStringField(root, "cmd", bag.cmd);
    if (json_object_object_get_ex(root, "seq", &obj)) {
        bag.seq = json_object_get_int64(obj);
    }
    if (json_object_object_get_ex(root, "timestamp", &obj)) {
        bag.timestamp = json_object_get_int64(obj);
    }
}

}  // namespace

struct json_object* FirmwareEntry::toJsonObject() const {
    struct json_object* fwObj = json_object_new_object();
    json_object_object_add(fwObj, "path", json_object_new_string(path.c_str()));
    json_object_object_add(fwObj, "name", json_object_new_string(name.c_str()));
    json_object_object_add(fwObj, "size", json_object_new_int64(size));
    json_object_object_add(fwObj, "version", json_object_new_string(version.c_str()));
    json_object_object_add(fwObj, "pack_time", json_object_new_string(packTime.c_str()));
    json_object_object_add(fwObj, "changelog", json_object_new_string(changelog.c_str()));

    struct json_object* execArr = json_object_new_array();
    for (const std::string& exe : executables) {
        json_object_array_add(execArr, json_object_new_string(exe.c_str()));
    }
    json_object_object_add(fwObj, "executables", execArr);

    struct json_object* filesArr = json_object_new_array();
    for (const FirmwareFileInfo& file : files) {
        struct json_object* fileObj = json_object_new_object();
        json_object_object_add(fileObj, "name", json_object_new_string(file.name.c_str()));
        json_object_object_add(fileObj, "size", json_object_new_int64(file.size));
        json_object_array_add(filesArr, fileObj);
    }
    json_object_object_add(fwObj, "files", filesArr);
    return fwObj;
}

bool RequestPushContentBag::checkValid() {
    return source == "client" && cmd == "request_push_content_to_embedded"
        && !deviceUids.empty() && !paths.empty();
}

struct json_object* RequestPushContentBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "device_uids", stringArrayToJson(deviceUids));
    json_object_object_add(paramsObj, "paths", stringArrayToJson(paths));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* RequestPushContentBag::toJsonString() { return bagToString(*this); }

void RequestPushContentBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringArray(paramsObj, "device_uids", deviceUids);
        readStringArray(paramsObj, "paths", paths);
    }
    json_object_put(root);
}

bool PushResourcesToDownloadBag::checkValid() {
    return source == "server" && cmd == "push_resources_to_download" && !paths.empty();
}

struct json_object* PushResourcesToDownloadBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "paths", stringArrayToJson(paths));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* PushResourcesToDownloadBag::toJsonString() { return bagToString(*this); }

void PushResourcesToDownloadBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringArray(paramsObj, "paths", paths);
    }
    json_object_put(root);
}

bool RequestSchedulePushBag::checkValid() {
    return source == "client" && cmd == "request_schedule_push"
        && !deviceUids.empty() && !paths.empty()
        && !scheduleDate.empty() && !scheduleTime.empty() && durationSec > 0;
}

struct json_object* RequestSchedulePushBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "device_uids", stringArrayToJson(deviceUids));
    json_object_object_add(paramsObj, "paths", stringArrayToJson(paths));
    json_object_object_add(paramsObj, "schedule_date", json_object_new_string(scheduleDate.c_str()));
    json_object_object_add(paramsObj, "schedule_time", json_object_new_string(scheduleTime.c_str()));
    json_object_object_add(paramsObj, "duration_sec", json_object_new_int(durationSec));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* RequestSchedulePushBag::toJsonString() { return bagToString(*this); }

void RequestSchedulePushBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* obj = nullptr;
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringArray(paramsObj, "device_uids", deviceUids);
        readStringArray(paramsObj, "paths", paths);
        readStringField(paramsObj, "schedule_date", scheduleDate);
        readStringField(paramsObj, "schedule_time", scheduleTime);
        if (json_object_object_get_ex(paramsObj, "duration_sec", &obj)) {
            durationSec = json_object_get_int(obj);
        }
    }
    json_object_put(root);
}

bool PushSchedulePlaylistBag::checkValid() {
    return source == "server" && cmd == "push_schedule_playlist"
        && !paths.empty() && !scheduleDate.empty() && !scheduleTime.empty() && durationSec > 0;
}

struct json_object* PushSchedulePlaylistBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "paths", stringArrayToJson(paths));
    json_object_object_add(paramsObj, "schedule_date", json_object_new_string(scheduleDate.c_str()));
    json_object_object_add(paramsObj, "schedule_time", json_object_new_string(scheduleTime.c_str()));
    json_object_object_add(paramsObj, "duration_sec", json_object_new_int(durationSec));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* PushSchedulePlaylistBag::toJsonString() { return bagToString(*this); }

void PushSchedulePlaylistBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* obj = nullptr;
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringArray(paramsObj, "paths", paths);
        readStringField(paramsObj, "schedule_date", scheduleDate);
        readStringField(paramsObj, "schedule_time", scheduleTime);
        if (json_object_object_get_ex(paramsObj, "duration_sec", &obj)) {
            durationSec = json_object_get_int(obj);
        }
    }
    json_object_put(root);
}

bool RequestFirmwareListBag::checkValid() {
    return source == "client" && cmd == "request_firmware_list";
}

struct json_object* RequestFirmwareListBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    json_object_object_add(root, "params", json_object_new_object());
    return root;
}

char* RequestFirmwareListBag::toJsonString() { return bagToString(*this); }

void RequestFirmwareListBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    json_object_put(root);
}

bool RequestFirmwareListAckBag::checkValid() {
    return source == "server" && cmd == "request_firmware_list_ack";
}

struct json_object* RequestFirmwareListAckBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "count", json_object_new_int(count));
    struct json_object* arr = json_object_new_array();
    for (const FirmwareEntry& fw : firmwares) {
        json_object_array_add(arr, fw.toJsonObject());
    }
    json_object_object_add(paramsObj, "firmwares", arr);
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* RequestFirmwareListAckBag::toJsonString() { return bagToString(*this); }

void RequestFirmwareListAckBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* obj = nullptr;
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        if (json_object_object_get_ex(paramsObj, "count", &obj)) {
            count = json_object_get_int(obj);
        }
    }
    json_object_put(root);
}

bool RequestOtaUpdateBag::checkValid() {
    return source == "client" && cmd == "request_ota_update"
        && !deviceUids.empty() && !path.empty() && !clientUid.empty();
}

struct json_object* RequestOtaUpdateBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "device_uids", stringArrayToJson(deviceUids));
    json_object_object_add(paramsObj, "path", json_object_new_string(path.c_str()));
    json_object_object_add(paramsObj, "device_uid", json_object_new_string(clientUid.c_str()));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* RequestOtaUpdateBag::toJsonString() { return bagToString(*this); }

void RequestOtaUpdateBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringArray(paramsObj, "device_uids", deviceUids);
        readStringField(paramsObj, "path", path);
        readStringField(paramsObj, "device_uid", clientUid);
    }
    json_object_put(root);
}

bool OtaUpdateBag::checkValid() {
    return source == "server" && cmd == "ota_update"
        && !path.empty() && !md5.empty() && !clientUid.empty();
}

struct json_object* OtaUpdateBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "path", json_object_new_string(path.c_str()));
    json_object_object_add(paramsObj, "md5", json_object_new_string(md5.c_str()));
    json_object_object_add(paramsObj, "device_uid", json_object_new_string(clientUid.c_str()));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* OtaUpdateBag::toJsonString() { return bagToString(*this); }

void OtaUpdateBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringField(paramsObj, "path", path);
        readStringField(paramsObj, "md5", md5);
        readStringField(paramsObj, "device_uid", clientUid);
    }
    json_object_put(root);
}

bool EmbeddedOtaUpdateAckBag::checkValid() {
    return source == "embedded" && cmd == "ota_update_ack" && !clientUid.empty();
}

struct json_object* EmbeddedOtaUpdateAckBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "code", json_object_new_int(code));
    json_object_object_add(paramsObj, "result", json_object_new_int(result));
    json_object_object_add(paramsObj, "msg", json_object_new_string(msg.c_str()));
    json_object_object_add(paramsObj, "path", json_object_new_string(path.c_str()));
    json_object_object_add(paramsObj, "local_path", json_object_new_string(localPath.c_str()));
    json_object_object_add(paramsObj, "device_uid", json_object_new_string(clientUid.c_str()));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* EmbeddedOtaUpdateAckBag::toJsonString() { return bagToString(*this); }

void EmbeddedOtaUpdateAckBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* obj = nullptr;
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        if (json_object_object_get_ex(paramsObj, "code", &obj)) code = json_object_get_int(obj);
        if (json_object_object_get_ex(paramsObj, "result", &obj)) result = json_object_get_int(obj);
        readStringField(paramsObj, "msg", msg);
        readStringField(paramsObj, "path", path);
        readStringField(paramsObj, "local_path", localPath);
        readStringField(paramsObj, "device_uid", clientUid);
    }
    json_object_put(root);
}

bool ClientOtaUpdateAckBag::checkValid() {
    return source == "server" && cmd == "ota_update_ack";
}

struct json_object* ClientOtaUpdateAckBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "result", json_object_new_int(result));
    if (!path.empty()) {
        json_object_object_add(paramsObj, "path", json_object_new_string(path.c_str()));
    }
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* ClientOtaUpdateAckBag::toJsonString() { return bagToString(*this); }

void ClientOtaUpdateAckBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* obj = nullptr;
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        if (json_object_object_get_ex(paramsObj, "result", &obj)) result = json_object_get_int(obj);
        readStringField(paramsObj, "path", path);
    }
    json_object_put(root);
}

bool RequestCheckFirmwareBag::checkValid() {
    return source == "client" && cmd == "request_check_firmware"
        && !path.empty() && !md5.empty();
}

struct json_object* RequestCheckFirmwareBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "path", json_object_new_string(path.c_str()));
    json_object_object_add(paramsObj, "md5", json_object_new_string(md5.c_str()));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* RequestCheckFirmwareBag::toJsonString() { return bagToString(*this); }

void RequestCheckFirmwareBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        readStringField(paramsObj, "path", path);
        readStringField(paramsObj, "md5", md5);
    }
    json_object_put(root);
}

bool CheckFirmwareAckBag::checkValid() {
    return source == "server" && cmd == "check_firmware_ack";
}

struct json_object* CheckFirmwareAckBag::toJsonObject() {
    if (!checkValid()) return nullptr;
    struct json_object* root = packHead();
    struct json_object* paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "result", json_object_new_int(result));
    json_object_object_add(root, "params", paramsObj);
    return root;
}

char* CheckFirmwareAckBag::toJsonString() { return bagToString(*this); }

void CheckFirmwareAckBag::loadFromJsonString(char* str) {
    if (!str) return;
    struct json_object* root = json_tokener_parse(str);
    if (!root) return;
    loadBagHead(root, *this);
    struct json_object* obj = nullptr;
    struct json_object* paramsObj = nullptr;
    if (json_object_object_get_ex(root, "params", &paramsObj)
        && json_object_is_type(paramsObj, json_type_object)) {
        if (json_object_object_get_ex(paramsObj, "result", &obj)) result = json_object_get_int(obj);
    }
    json_object_put(root);
}
