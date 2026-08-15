#ifndef JSON_BAGS_EXT_H
#define JSON_BAGS_EXT_H

#include "json_bags.h"
#include <vector>

struct FirmwareFileInfo {
    std::string name;
    int64_t     size = 0;
};

struct FirmwareEntry {
    std::string path;
    std::string name;
    int64_t     size = 0;
    std::string version;
    std::string packTime;
    std::string changelog;
    std::vector<std::string> executables;
    std::vector<FirmwareFileInfo> files;

    struct json_object* toJsonObject() const;
};

struct RequestPushContentBag: public JsonBag {
    std::vector<std::string> deviceUids;
    std::vector<std::string> paths;

    RequestPushContentBag(): JsonBag("client") { cmd = "request_push_content_to_embedded"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct PushResourcesToDownloadBag: public JsonBag {
    std::vector<std::string> paths;

    PushResourcesToDownloadBag(): JsonBag("server") { cmd = "push_resources_to_download"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct RequestSchedulePushBag: public JsonBag {
    std::vector<std::string> deviceUids;
    std::vector<std::string> paths;
    std::string scheduleDate;
    std::string scheduleTime;
    int         durationSec = 0;

    RequestSchedulePushBag(): JsonBag("client") { cmd = "request_schedule_push"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct PushSchedulePlaylistBag: public JsonBag {
    std::vector<std::string> paths;
    std::string scheduleDate;
    std::string scheduleTime;
    int         durationSec = 0;

    PushSchedulePlaylistBag(): JsonBag("server") { cmd = "push_schedule_playlist"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct RequestFirmwareListBag: public JsonBag {
    RequestFirmwareListBag(): JsonBag("client") { cmd = "request_firmware_list"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct RequestFirmwareListAckBag: public JsonBag {
    int count = 0;
    std::vector<FirmwareEntry> firmwares;

    RequestFirmwareListAckBag(): JsonBag("server") { cmd = "request_firmware_list_ack"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct RequestOtaUpdateBag: public JsonBag {
    std::vector<std::string> deviceUids;
    std::string path;
    std::string clientUid;

    RequestOtaUpdateBag(): JsonBag("client") { cmd = "request_ota_update"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct OtaUpdateBag: public JsonBag {
    std::string path;
    std::string md5;
    std::string clientUid;

    OtaUpdateBag(): JsonBag("server") { cmd = "ota_update"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct EmbeddedOtaUpdateAckBag: public JsonBag {
    int         code = -1;
    int         result = 0;
    std::string msg;
    std::string path;
    std::string localPath;
    std::string clientUid;

    EmbeddedOtaUpdateAckBag(): JsonBag("embedded") { cmd = "ota_update_ack"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct ClientOtaUpdateAckBag: public JsonBag {
    int         result = 0;
    std::string path;

    ClientOtaUpdateAckBag(): JsonBag("server") { cmd = "ota_update_ack"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct RequestCheckFirmwareBag: public JsonBag {
    std::string path;
    std::string md5;

    RequestCheckFirmwareBag(): JsonBag("client") { cmd = "request_check_firmware"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

struct CheckFirmwareAckBag: public JsonBag {
    int result = 0;

    CheckFirmwareAckBag(): JsonBag("server") { cmd = "check_firmware_ack"; }
    bool checkValid() override;
    struct json_object* toJsonObject() override;
    char* toJsonString() override;
    void loadFromJsonString(char* str) override;
};

#endif
