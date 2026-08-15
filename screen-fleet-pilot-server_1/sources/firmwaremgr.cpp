#include "../includes/firmwaremgr.h"
#include "global_def.h"
#include "logmgr.h"
#include <cctype>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

static LogMgr* logger = &LogMgr::getInstance();
static const size_t MAX_MANIFEST_BYTES = 16384;

namespace {

std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        out += (c == '\'') ? "'\\''" : std::string(1, c);
    }
    out += "'";
    return out;
}

bool isSafeBasename(const std::string& name) {
    return !name.empty() && name.find('/') == std::string::npos && name.find("..") == std::string::npos;
}

bool isFirmwareArchive(const std::string& name) {
    if (name.size() >= 7 && name.compare(name.size() - 7, 7, ".tar.gz") == 0) return true;
    if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".tgz") == 0) return true;
    if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".tar") == 0) return true;
    return false;
}

std::string tarBasename(const std::string& path) {
    size_t pos = path.rfind('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string readPipeLimited(FILE* fp, size_t maxBytes) {
    std::string buf;
    char chunk[4096];
    while (maxBytes > 0) {
        size_t toRead = maxBytes > sizeof(chunk) ? sizeof(chunk) : maxBytes;
        size_t n = fread(chunk, 1, toRead, fp);
        if (n == 0) break;
        buf.append(chunk, n);
        maxBytes -= n;
    }
    return buf;
}

std::vector<std::string> tarListMembers(const std::string& tgzPath) {
    std::vector<std::string> members;
    std::string cmd = "tar -tzf " + shellQuote(tgzPath) + " 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return members;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        std::string member(line);
        while (!member.empty() && (member.back() == '\n' || member.back() == '\r')) member.pop_back();
        if (!member.empty()) members.push_back(member);
    }
    pclose(fp);
    return members;
}

std::string tarExtractMember(const std::string& tgzPath, const std::string& member, size_t maxBytes) {
    std::string cmd = "tar -xOf " + shellQuote(tgzPath) + " " + shellQuote(member) + " 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return "";
    std::string content = readPipeLimited(fp, maxBytes);
    pclose(fp);
    return content;
}

std::string findReadmeJsonMember(const std::vector<std::string>& members) {
    std::string best;
    for (const std::string& member : members) {
        if (tarBasename(member) != "README.json") continue;
        if (best.empty() || member.size() < best.size()) best = member;
    }
    return best;
}

bool parseFirmwareManifest(const std::string& jsonText, FirmwareEntry& entry) {
    if (jsonText.empty()) return false;
    struct json_object* root = json_tokener_parse(jsonText.c_str());
    if (!root) return false;

    struct json_object* obj = nullptr;
    if (json_object_object_get_ex(root, "version", &obj) && json_object_is_type(obj, json_type_string)) {
        entry.version = json_object_get_string(obj);
    }
    if (json_object_object_get_ex(root, "pack_time", &obj) && json_object_is_type(obj, json_type_string)) {
        entry.packTime = json_object_get_string(obj);
    }
    if (json_object_object_get_ex(root, "changelog", &obj) && json_object_is_type(obj, json_type_string)) {
        entry.changelog = json_object_get_string(obj);
    }
    if (json_object_object_get_ex(root, "executables", &obj) && json_object_is_type(obj, json_type_array)) {
        int len = json_object_array_length(obj);
        for (int i = 0; i < len; i++) {
            struct json_object* item = json_object_array_get_idx(obj, i);
            if (item && json_object_is_type(item, json_type_string)) {
                entry.executables.push_back(json_object_get_string(item));
            }
        }
    }
    if (json_object_object_get_ex(root, "files", &obj) && json_object_is_type(obj, json_type_array)) {
        int len = json_object_array_length(obj);
        for (int i = 0; i < len; i++) {
            struct json_object* item = json_object_array_get_idx(obj, i);
            if (!item || !json_object_is_type(item, json_type_object)) continue;
            FirmwareFileInfo info;
            struct json_object* nameObj = nullptr;
            struct json_object* sizeObj = nullptr;
            json_object_object_get_ex(item, "name", &nameObj);
            json_object_object_get_ex(item, "size", &sizeObj);
            if (nameObj && json_object_is_type(nameObj, json_type_string)) {
                info.name = json_object_get_string(nameObj);
            }
            if (sizeObj) info.size = json_object_get_int64(sizeObj);
            if (!info.name.empty()) entry.files.push_back(info);
        }
    }
    json_object_put(root);
    return !entry.version.empty();
}

FirmwareEntry inspectFirmwareArchive(const std::string& fullPath, const std::string& fileName, long long fileSize) {
    FirmwareEntry entry;
    entry.name = fileName;
    entry.path = std::string(FIRMWARE_URL_PREFIX) + "/" + fileName;
    entry.size = fileSize;

    std::vector<std::string> members = tarListMembers(fullPath);
    std::string manifestMember = findReadmeJsonMember(members);
    if (manifestMember.empty()) {
        logger->logMsg(WARNING, "firmware missing README.json: " + fileName, true);
        return entry;
    }
    std::string manifestJson = tarExtractMember(fullPath, manifestMember, MAX_MANIFEST_BYTES);
    if (!parseFirmwareManifest(manifestJson, entry)) {
        logger->logMsg(WARNING, "firmware README.json parse failed: " + fileName, true);
    }
    return entry;
}

}  // namespace

void FirmwareMgr::loadFirmwareList(std::vector<FirmwareEntry>& firmwares) {
    firmwares.clear();
    DIR* dir = opendir(FIRMWARE_DIR);
    if (!dir) {
        logger->logMsg(ERROR, std::string("open firmware dir failed: ") + FIRMWARE_DIR, true);
        return;
    }

    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        if (!isSafeBasename(ent->d_name) || !isFirmwareArchive(ent->d_name)) continue;

        const std::string fullPath = std::string(FIRMWARE_DIR) + "/" + ent->d_name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
        firmwares.push_back(inspectFirmwareArchive(fullPath, ent->d_name, static_cast<long long>(st.st_size)));
    }
    closedir(dir);
}

std::string FirmwareMgr::computeFileMd5(const std::string& filePath) {
    std::string cmd = "md5sum " + shellQuote(filePath) + " 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return "";
    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return "";
    }
    pclose(fp);
    std::string line(buf);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    return line.size() >= 32 ? line.substr(0, 32) : "";
}

bool FirmwareMgr::resolveFirmwarePath(const std::string& pathStr, std::string& firmwarePath) {
    const std::string prefix = std::string(FIRMWARE_URL_PREFIX) + "/";
    if (pathStr.compare(0, prefix.size(), prefix) != 0) return false;
    const std::string name = pathStr.substr(prefix.size());
    if (!isSafeBasename(name) || !isFirmwareArchive(name)) return false;
    firmwarePath = std::string(FIRMWARE_DIR) + "/" + name;
    struct stat st;
    return stat(firmwarePath.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool FirmwareMgr::checkFirmwareMd5(const std::string& relativePath, const std::string& expectedMd5) {
    std::string fullPath;
    if (!resolveFirmwarePath(relativePath, fullPath)) return false;
    std::string actual = computeFileMd5(fullPath);
    if (actual.empty() || expectedMd5.size() != 32) return false;
    for (size_t i = 0; i < 32; i++) {
        if (tolower(static_cast<unsigned char>(actual[i]))
            != tolower(static_cast<unsigned char>(expectedMd5[i]))) {
            return false;
        }
    }
    return true;
}
