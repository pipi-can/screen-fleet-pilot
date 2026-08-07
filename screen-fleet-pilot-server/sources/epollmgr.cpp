#include "../includes/epollmgr.h"
#include "../includes/databasemgr.h"
#include "../includes/schedulemgr.h"
#include <cctype>
#include <cstdio>

static LogMgr* logger = &LogMgr::getInstance();
static DatabaseMgr* dbMgr = &DatabaseMgr::getInstance();

static const char* FIRMWARE_DIR = "/var/www/firmwares";
static const size_t MAX_MANIFEST_BYTES = 16384;

static std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
}

static bool isSafeBasename(const std::string& name) {
    return !name.empty()
        && name.find('/') == std::string::npos
        && name.find("..") == std::string::npos;
}

static bool isFirmwareArchive(const std::string& name) {
    if (name.size() >= 7 && name.compare(name.size() - 7, 7, ".tar.gz") == 0) {
        return true;
    }
    if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".tgz") == 0) {
        return true;
    }
    if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".tar") == 0) {
        return true;
    }
    return false;
}

static bool isHexMd5(const std::string& s) {
    if (s.size() != 32) {
        return false;
    }
    for (char c : s) {
        if (!isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

static bool md5EqualsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (tolower(static_cast<unsigned char>(a[i]))
            != tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

static std::string computeFileMd5(const std::string& filePath) {
    std::string cmd = "md5sum " + shellQuote(filePath) + " 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        return "";
    }

    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return "";
    }
    pclose(fp);

    std::string line(buf);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    if (line.size() < 32) {
        return "";
    }
    return line.substr(0, 32);
}

static bool resolveFirmwarePath(const std::string& pathStr, std::string& firmwarePath) {
    const std::string prefix = "/firmwares/";
    if (pathStr.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }

    const std::string firmwareName = pathStr.substr(prefix.size());
    if (!isSafeBasename(firmwareName) || !isFirmwareArchive(firmwareName)) {
        return false;
    }

    firmwarePath = std::string(FIRMWARE_DIR) + "/" + firmwareName;
    struct stat st;
    if (stat(firmwarePath.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }
    return true;
}

static std::string tarBasename(const std::string& path) {
    size_t pos = path.rfind('/');
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

static std::string readPipeLimited(FILE* fp, size_t maxBytes) {
    std::string buf;
    char chunk[4096];
    while (maxBytes > 0) {
        size_t toRead = maxBytes > sizeof(chunk) ? sizeof(chunk) : maxBytes;
        size_t n = fread(chunk, 1, toRead, fp);
        if (n == 0) {
            break;
        }
        buf.append(chunk, n);
        maxBytes -= n;
    }
    return buf;
}

static std::vector<std::string> tarListMembers(const std::string& tgzPath) {
    std::vector<std::string> members;
    std::string cmd = "tar -tzf " + shellQuote(tgzPath) + " 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        return members;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        std::string member(line);
        while (!member.empty() && (member.back() == '\n' || member.back() == '\r')) {
            member.pop_back();
        }
        if (!member.empty()) {
            members.push_back(member);
        }
    }
    pclose(fp);
    return members;
}

static std::string tarExtractMember(const std::string& tgzPath,
                                    const std::string& member,
                                    size_t maxBytes) {
    std::string cmd = "tar -xOf " + shellQuote(tgzPath) + " "
                    + shellQuote(member) + " 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        return "";
    }
    std::string content = readPipeLimited(fp, maxBytes);
    pclose(fp);
    return content;
}

static std::string findReadmeJsonMember(const std::vector<std::string>& members) {
    std::string best;
    for (const std::string& member : members) {
        if (tarBasename(member) != "README.json") {
            continue;
        }
        if (best.empty() || member.size() < best.size()) {
            best = member;
        }
    }
    return best;
}

static bool parseFirmwareManifest(const std::string& jsonText, FirmwareEntry& entry) {
    if (jsonText.empty()) {
        return false;
    }

    struct json_object* root = json_tokener_parse(jsonText.c_str());
    if (!root) {
        return false;
    }

    struct json_object* verObj = NULL;
    if (json_object_object_get_ex(root, "version", &verObj)
        && json_object_get_type(verObj) == json_type_string) {
        entry.version = json_object_get_string(verObj);
    }

    struct json_object* packTimeObj = NULL;
    if (json_object_object_get_ex(root, "pack_time", &packTimeObj)
        && json_object_get_type(packTimeObj) == json_type_string) {
        entry.packTime = json_object_get_string(packTimeObj);
    }

    struct json_object* changelogObj = NULL;
    if (json_object_object_get_ex(root, "changelog", &changelogObj)
        && json_object_get_type(changelogObj) == json_type_string) {
        entry.changelog = json_object_get_string(changelogObj);
    }

    struct json_object* execObj = NULL;
    if (json_object_object_get_ex(root, "executables", &execObj)
        && json_object_get_type(execObj) == json_type_array) {
        int len = json_object_array_length(execObj);
        for (int i = 0; i < len; i++) {
            struct json_object* item = json_object_array_get_idx(execObj, i);
            if (item && json_object_get_type(item) == json_type_string) {
                entry.executables.push_back(json_object_get_string(item));
            }
        }
    }

    struct json_object* filesObj = NULL;
    if (json_object_object_get_ex(root, "files", &filesObj)
        && json_object_get_type(filesObj) == json_type_array) {
        int len = json_object_array_length(filesObj);
        for (int i = 0; i < len; i++) {
            struct json_object* item = json_object_array_get_idx(filesObj, i);
            if (!item || json_object_get_type(item) != json_type_object) {
                continue;
            }
            FirmwareFileInfo info;
            struct json_object* nameObj = NULL;
            struct json_object* sizeObj = NULL;
            json_object_object_get_ex(item, "name", &nameObj);
            json_object_object_get_ex(item, "size", &sizeObj);
            if (nameObj && json_object_get_type(nameObj) == json_type_string) {
                info.name = json_object_get_string(nameObj);
            }
            if (sizeObj) {
                info.size = json_object_get_int64(sizeObj);
            }
            if (!info.name.empty()) {
                entry.files.push_back(info);
            }
        }
    }

    json_object_put(root);
    return !entry.version.empty();
}

static struct json_object* firmwareEntryToJson(const FirmwareEntry& fw) {
    struct json_object* fwObj = json_object_new_object();
    const std::string serverPath = std::string("/firmwares/") + fw.fileName;

    json_object_object_add(fwObj, "path", json_object_new_string(serverPath.c_str()));
    json_object_object_add(fwObj, "name", json_object_new_string(fw.fileName.c_str()));
    json_object_object_add(fwObj, "size", json_object_new_int64(fw.fileSize));
    json_object_object_add(fwObj, "version", json_object_new_string(fw.version.c_str()));
    json_object_object_add(fwObj, "pack_time", json_object_new_string(fw.packTime.c_str()));
    json_object_object_add(fwObj, "changelog", json_object_new_string(fw.changelog.c_str()));

    struct json_object* execArr = json_object_new_array();
    for (const std::string& exe : fw.executables) {
        json_object_array_add(execArr, json_object_new_string(exe.c_str()));
    }
    json_object_object_add(fwObj, "executables", execArr);

    struct json_object* filesArr = json_object_new_array();
    for (const FirmwareFileInfo& file : fw.files) {
        struct json_object* fileObj = json_object_new_object();
        json_object_object_add(fileObj, "name", json_object_new_string(file.name.c_str()));
        json_object_object_add(fileObj, "size", json_object_new_int64(file.size));
        json_object_array_add(filesArr, fileObj);
    }
    json_object_object_add(fwObj, "files", filesArr);

    return fwObj;
}

static FirmwareEntry inspectFirmwareArchive(const std::string& fullPath,
                                            const std::string& fileName,
                                            long long fileSize) {
    FirmwareEntry entry;
    entry.filePath = fullPath;
    entry.fileName = fileName;
    entry.fileSize = fileSize;

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

DeviceInfo::DeviceInfo() : m_deviceTimestamp(0) {

}
DeviceInfo::DeviceInfo(std::string name, std::string group, std::string version, DeviceType type) : 
    m_name(name), m_group(group), m_version(version), m_type(type),
    m_lastUploadTime(time(NULL)), m_deviceTimestamp(0) {

}

std::string DeviceInfo::name() {
    return this->m_name;
}

std::string DeviceInfo::group() {
    return this->m_group;
}

std::string DeviceInfo::version() {
    return this->m_version;
}

time_t DeviceInfo::lastUploadTime() {
    return this->m_lastUploadTime;
}

DeviceType DeviceInfo::type() {
    return this->m_type;
}

std::string DeviceInfo::temperature() {
    return this->m_temperature;
}

int DeviceInfo::memUsage() {
    return this->m_memUsage;
}

int DeviceInfo::diskFreeMb() {
    return this->m_diskFreeMb;
}


std::set<std::string> DeviceInfo::maskUidList() {
    return this->m_maskUidList;
}

void DeviceInfo::setId(int id) {
    this->m_id = id;
}

void DeviceInfo::updateLastUploadTime() {
    this->m_lastUploadTime = time(NULL);
}

void DeviceInfo::setGroup(const char* group) {
    this->m_group = group;
}

void DeviceInfo::setName(const char* name) {
    this->m_name = name;
}

void DeviceInfo::setTemperature(const std::string& temperature) {
    this->m_temperature = temperature;
}

void DeviceInfo::setMemUsage(int memUsage) {
    this->m_memUsage = memUsage;
}

void DeviceInfo::setDiskFreeMb(int diskFreeMb) {
    this->m_diskFreeMb = diskFreeMb;
}   

int DeviceInfo::id() {
    return this->m_id;
}

std::string DeviceInfo::deviceUid() {
    return this->m_deviceUid;
}

void DeviceInfo::setDeviceUid(const std::string& uid) {
    this->m_deviceUid = uid;
}

time_t DeviceInfo::deviceTimestamp() {
    return this->m_deviceTimestamp;
}

void DeviceInfo::setDeviceTimestamp(time_t ts) {
    this->m_deviceTimestamp = ts;
}

void DeviceInfo::addAdvToList(const std::string& adv) {
    this->m_advList.push_back(adv);
}

void DeviceInfo::addAdvsToList(std::vector<std::string> advs) {
    for (std::string& adv : advs) {
        this->m_advList.push_back(adv);
    }
}

void DeviceInfo::addMask(const std::string& uid) {
    this->m_maskUidList.insert(uid);
}

EpollManager::EpollManager() {

}

EpollManager::~EpollManager() {

}

EpollManager& EpollManager::getInstance() {
    static EpollManager instance;
    return instance;
}

void EpollManager::init() {
    m_deviceCnt = 0;
    m_epollFd = epoll_create(1);
    if (m_epollFd < 0) {
        logger->logMsg(ERROR, "epoll init failed", true);
        return ;
    } 
    logger->logMsg(DEBUG, "epoll init success", true);
}

void EpollManager::add(int fd, EPOLL_EVENTS event) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = event;
    epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev);
}

int EpollManager::getEpollFd() {
    return m_epollFd;
}

void EpollManager::wait() {
    logger->logMsg(DEBUG, "epoll waitting......", true);
    struct epoll_event evs[MAX_CONNECTIONS];
    memset(evs, 0, sizeof(evs));

    while (1) {
        int cnt = epoll_wait(m_epollFd, evs, MAX_CONNECTIONS, 1000);
        if (cnt < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                logger->logMsg(ERROR, "epoll wait error!", true);
                return ;
            }
        }
        
        for (int i = 0; i < cnt; i++) {
            struct epoll_event ev = evs[i];
            int fd = ev.data.fd;
            if (fd == SocketMgr::getInstance().getSocketFd()) {
                // new client connected
                logger->logMsg(DEBUG, "new client try to connect to host...", true);

                handleNewClient();
            } else if (fd == ScheduleMgr::getInstance().timerFd()) {
                ScheduleMgr::getInstance().onTimerExpired();
            } else {
                // ET: 每次 EPOLLIN 必须读到 EAGAIN
                logger->logMsg(DEBUG, "client send message to read......", true);

                char buf[4096];
                while (true) {
                    int n = recv(fd, buf, sizeof(buf), 0);
                    if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        logger->logMsg(ERROR, "client link error", true);
                        recycleClient(fd);
                        break;
                    } else if (n == 0) {
                        logger->logMsg(DEBUG, "client close link", true);
                        recycleClient(fd);
                        break;
                    }

                    FdBuffer& fdbuf = m_fd2Buffer[fd];
                    if (fdbuf.append(buf, n) < 0) {
                        logger->logMsg(ERROR, "client buffer overflow", true);
                        recycleClient(fd);
                        break;
                    }
                    while (true) {
                        char* nl = (char*)memchr(fdbuf.data, '\n', fdbuf.len);
                        if (!nl) break;

                        int frameLen = nl - fdbuf.data;
                        if (frameLen > 0 && fdbuf.data[frameLen - 1] == '\r') {
                            frameLen--;
                        }
                        if (frameLen > 0) {
                            fdbuf.data[frameLen] = '\0';
                            parseMessage(fd, fdbuf.data);
                        }
                        fdbuf.consume(frameLen + 1);
                    }
                }
            }
        }

        checkTimeout();
    }
}

void EpollManager::sendJson(int fd, struct json_object* obj) {
    const char* data = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
    std::string packet = std::string(data) + "\n";
    send(fd, packet.c_str(), packet.size(), 0);
}

void EpollManager::parseMessage(int fd, char* message) {
    struct json_object* root = json_tokener_parse(message);
    if (!root) {
        logger->logMsg(ERROR, "json parse failed", true);
        return;
    }

    struct json_object* sourceObj = NULL;
    if (!json_object_object_get_ex(root, "source", &sourceObj)) {
        logger->logMsg(ERROR, "missing source field", true);
        json_object_put(root);
        return;
    }

    const char* source = json_object_get_string(sourceObj);

    if (strcmp(source, "embedded") == 0) {
        handleEmbeddedMessage(fd, root);
    } else if (strcmp(source, "client") == 0) {
        handleClientMessage(fd, root);  
    } else {
        logger->logMsg(WARNING, "unknown source", true);
    }

    json_object_put(root);
}

void EpollManager::handleEmbeddedMessage(int fd, struct json_object* root) {
    struct json_object* cmdObj = NULL;
    if (!json_object_object_get_ex(root, "cmd", &cmdObj)) {
        logger->logMsg(ERROR, "missing cmd field", true);
        return;
    }
    const char* cmd = json_object_get_string(cmdObj);

    struct json_object* paramsObj = NULL;
    json_object_object_get_ex(root, "params", &paramsObj);

    if (strcmp(cmd, "register") == 0) {
        handleEmbeddedRegister(fd, paramsObj);
    } else if (strcmp(cmd, "heartbeat") == 0) {
        handleEmbeddedHeartbeat(fd, root, paramsObj);
    } else if (strcmp(cmd, "update_info_ack") == 0) {
        handleEmbeddedUpdateInfoAck(fd, paramsObj);
    } else if (strcmp(cmd, "screenshot_data") == 0) {
        handleEmbeddedScreenshotData(fd, paramsObj);
    } else if (strcmp(cmd, "ota_update_ack") == 0) {
        handleEmbeddedOtaUpdateAck(fd, paramsObj);
    }
}

void EpollManager::handleClientMessage(int fd, struct json_object* root) {
    struct json_object* cmdObj = NULL;
    if (!json_object_object_get_ex(root, "cmd", &cmdObj)) {
        logger->logMsg(ERROR, "missing cmd field", true);
        return;
    }
    const char* cmd = json_object_get_string(cmdObj);

    struct json_object* paramsObj = NULL;
    json_object_object_get_ex(root, "params", &paramsObj);

    if (strcmp(cmd, "register") == 0) {
        handleClientRegister(fd, paramsObj);
    } else if (strcmp(cmd, "fetch_devices") == 0) {
        handleClientFetchDevices(fd);
    } else if (strcmp(cmd, "heartbeat") == 0) {
        handleClientHeartbeat(fd, root);
    } else if (strcmp(cmd, "request_update_embedded") == 0) {
        handleClientRequestUpdateEmbedded(fd, paramsObj);
    } else if (strcmp(cmd, "request_file_list") == 0) {
        handleClientRequestFileList(fd, paramsObj);
    } else if (strcmp(cmd, "request_firmware_list") == 0) {
        handleClientRequestFirmwareList(fd, paramsObj);
    } else if (strcmp(cmd, "mask_device") == 0) {
        handleClientMaskDevice(fd, paramsObj);
    } else if (strcmp(cmd, "request_push_content_to_embedded") == 0) {
        handleClientRequestPushContentToEmbedded(fd, paramsObj);
    } else if (strcmp(cmd, "request_screenshot") == 0) {
        handleClientRequestScreenshot(fd, paramsObj);
    } else if (strcmp(cmd, "request_schedule_push") == 0) {
        handleClientRequestSchedulePush(fd, paramsObj);
    } else if (strcmp(cmd, "request_ota_update") == 0) {
        handleClientRequestOTAUpdate(fd, paramsObj);
    } else if (strcmp(cmd, "request_check_firmware") == 0) {
        handleClientRequestCheckFirmware(fd, paramsObj);
    }
}

void EpollManager::checkTimeout() {
    std::vector<int> dead;
    time_t now = time(NULL);
    for (auto& p : m_fd2DeviceMap) {
        if (now - p.second.lastUploadTime() > 30) {
            dead.push_back(p.second.id());
        }
    }
    // offline 
    for (int i = 0; i < dead.size(); i++) {
        int id = dead[i];
        auto fdIt = m_id2fdMap.find(id);
        if (fdIt == m_id2fdMap.end()) continue;
        int fd = fdIt->second;
        close(fd);
        m_fd2DeviceMap.erase(fd);
        m_id2fdMap.erase(id);
    }
}

void EpollManager::handleNewClient() {
    struct sockaddr_in clientInfo;
    while (true) {
        memset(&clientInfo, 0, sizeof(clientInfo));
        socklen_t sockLen = sizeof(clientInfo);
        int clientFd = accept(SocketMgr::getInstance().getSocketFd(), 
                                (struct sockaddr*)(&clientInfo), &sockLen);   
        
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // 连接队列已空，ET 要求读到这里
            }
            logger->logMsg(ERROR, "accept client failed", true);
            return;
        }
        
        if (!SocketMgr::setNonBlock(clientFd)) {
            logger->logMsg(ERROR, "set client nonblock failed", true);
            close(clientFd);
            continue;
        }
        add(clientFd, EPOLLIN | EPOLLET);
        m_fd2Buffer[clientFd] = FdBuffer();
    }
}

void EpollManager::recycleClient(int fd) {
    auto it = m_fd2DeviceMap.find(fd);
    if (it != m_fd2DeviceMap.end()) {
        m_id2fdMap.erase(it->second.id());
        m_fd2DeviceMap.erase(it);
    }
    m_fd2Buffer.erase(fd);
    close(fd);
}

void EpollManager::handleEmbeddedRegister(int fd, struct json_object* paramsObj) {
    logger->logMsg(DEBUG, "recieve register bag from embedded", true);
    if (!paramsObj) {
        logger->logMsg(ERROR, "register without params", true);
        return;
    }

    struct json_object *nameObj = NULL, *groupObj = NULL, *verObj = NULL;
    json_object_object_get_ex(paramsObj, "name",    &nameObj);
    json_object_object_get_ex(paramsObj, "group",   &groupObj);
    json_object_object_get_ex(paramsObj, "version", &verObj);

    const char* name    = nameObj  ? json_object_get_string(nameObj)  : "unknown";
    const char* group   = groupObj ? json_object_get_string(groupObj) : "default";
    const char* version = verObj   ? json_object_get_string(verObj)   : "1.0";

    struct json_object *uidObj = NULL;
    json_object_object_get_ex(paramsObj, "device_uid", &uidObj);
    std::string uid = uidObj ? json_object_get_string(uidObj) : std::string(name) + "@" + std::to_string(fd);

    DeviceInfo device(name, group, version, EMBEDDED);
    device.setDeviceUid(uid);

    int id;
    auto oldIt = m_uid2IdMap.find(uid);
    if (oldIt != m_uid2IdMap.end()) {
        id = oldIt->second;
        auto oldFdIt = m_id2fdMap.find(id);
        if (oldFdIt != m_id2fdMap.end()) {
            int oldFd = oldFdIt->second;
            if (oldFd != fd) {
                m_fd2DeviceMap.erase(oldFd);
                m_fd2Buffer.erase(oldFd);
                close(oldFd);
            }
        }
        logger->logMsg(DEBUG, "device reconnect with same uid", true);
    } else {
        id = ++m_deviceCnt;
        m_uid2IdMap[uid] = id;
    }
    device.setId(id);
    m_fd2DeviceMap[fd] = device;
    m_id2fdMap[id] = fd;

    if (!dbMgr->deviceExists(uid)) {
        dbMgr->insertDevice(uid, name, group, "embedded");
    }

    struct json_object* ack = json_object_new_object();
    json_object_object_add(ack, "source", json_object_new_string("server"));
    json_object_object_add(ack, "cmd",    json_object_new_string("register_ack"));
    json_object_object_add(ack, "seq",    json_object_new_int(0));

    struct json_object* ackParams = json_object_new_object();
    json_object_object_add(ackParams, "code",      json_object_new_int(0));
    json_object_object_add(ackParams, "device_id", json_object_new_int(id));
    json_object_object_add(ackParams, "msg",       json_object_new_string("ok"));
    json_object_object_add(ack, "params", ackParams);

    sendJson(fd, ack);
    json_object_put(ack);
    logger->logMsg(DEBUG, "send register ack to embedded", true);
}

void EpollManager::handleEmbeddedHeartbeat(int fd, struct json_object* root, struct json_object* paramsObj) {
    if (!paramsObj) {
        logger->logMsg(ERROR, "heartbeat without params", true);
        return;
    }

    struct json_object* idObj = NULL;
    json_object_object_get_ex(root, "device_id", &idObj);
    if (!idObj) {
        logger->logMsg(WARNING, "client do not have id!", true);
    } else {
        int id = json_object_get_int(idObj);
        std::string str = "get heartbeat from embedded: " + std::to_string(id);
        logger->logMsg(DEBUG, str.c_str(), true);
    }

    auto it = m_fd2DeviceMap.find(fd);
    if (it != m_fd2DeviceMap.end()) {
        it->second.updateLastUploadTime();
    } else {
        logger->logMsg(WARNING, "heartbeat from unknown embedded", true);
        return;
    }

    struct json_object* cpuTempObj = NULL, *memUsageObj = NULL, *diskFreeObj = NULL, *tsObj = NULL;
    json_object_object_get_ex(paramsObj, "cpu_temp", &cpuTempObj);
    json_object_object_get_ex(paramsObj, "mem_usage", &memUsageObj);
    json_object_object_get_ex(paramsObj, "disk_free_mb", &diskFreeObj);
    json_object_object_get_ex(paramsObj, "timestamp", &tsObj);
    const char* cpuTemp = cpuTempObj ? json_object_get_string(cpuTempObj) : "unknown";
    int memUsage = memUsageObj ? json_object_get_int(memUsageObj) : -1;
    int diskFree = diskFreeObj ? json_object_get_int(diskFreeObj) : -1;

    it->second.setTemperature(cpuTemp);
    it->second.setMemUsage(memUsage);
    it->second.setDiskFreeMb(diskFree);
    if (tsObj) {
        it->second.setDeviceTimestamp((time_t)json_object_get_int64(tsObj));
    }
}

void EpollManager::handleEmbeddedUpdateInfoAck(int fd, struct json_object* paramsObj) {
    if (!paramsObj) {
        logger->logMsg(ERROR, "update_info_ack without params", true);
        return;
    }

    struct json_object* statusObj = NULL, *senderObj = NULL, *nameObj = NULL, *groupObj = NULL;
    json_object_object_get_ex(paramsObj, "msg", &statusObj);
    json_object_object_get_ex(paramsObj, "sender", &senderObj);
    json_object_object_get_ex(paramsObj, "group", &groupObj);
    json_object_object_get_ex(paramsObj, "name", &nameObj);
    const char* status = statusObj ? json_object_get_string(statusObj) : "error";
    int senderFd = senderObj ? json_object_get_int(senderObj) : -1;
    const char* group = groupObj ? json_object_get_string(groupObj) : "unknown";
    const char* name = nameObj ? json_object_get_string(nameObj) : "unknown";

    if (senderFd < 0) {
        logger->logMsg(ERROR, "sender error!!!!!!!", true);
        return;
    }

    struct json_object* result = json_object_new_object();
    json_object_object_add(result, "source", json_object_new_string("server"));
    json_object_object_add(result, "cmd", json_object_new_string("update_embedded_info_result"));
    json_object_object_add(result, "seq",    json_object_new_int(0));

    struct json_object* param = json_object_new_object();
    json_object_object_add(param, "status", json_object_new_string(status));
    json_object_object_add(result, "params", param);

    if (strcmp(status, "ok") == 0) {
        auto it = m_fd2DeviceMap.find(fd);
        it->second.setGroup(group);
        it->second.setName(name);
        dbMgr->updateDevice(it->second.deviceUid(), name, group);
        logger->logMsg(DEBUG, "embedded update status success", true);
    } else {
        logger->logMsg(WARNING, "embedded update status failed", true);
    }

    sendJson(senderFd, result);
    json_object_put(result);
    logger->logMsg(DEBUG, "sent update_embedded_info_result success", true);
}

void EpollManager::handleClientRegister(int fd, struct json_object* paramsObj) {
    logger->logMsg(DEBUG, "recieve register bag from client", true);
    if (!paramsObj) {
        logger->logMsg(ERROR, "register without params", true);
        return;
    }

    struct json_object *nameObj = NULL, *groupObj = NULL, *verObj = NULL;
    json_object_object_get_ex(paramsObj, "name",    &nameObj);
    json_object_object_get_ex(paramsObj, "group",   &groupObj);
    json_object_object_get_ex(paramsObj, "version", &verObj);

    const char* name    = nameObj  ? json_object_get_string(nameObj)  : "unknown";
    const char* group   = groupObj ? json_object_get_string(groupObj) : "default";
    const char* version = verObj   ? json_object_get_string(verObj)   : "1.0";

    struct json_object *uidObj = NULL;
    json_object_object_get_ex(paramsObj, "device_uid", &uidObj);
    std::string uid = uidObj ? json_object_get_string(uidObj) : std::string(name) + "@" + std::to_string(fd);

    DeviceInfo device(name, group, version, CLIENT);
    device.setDeviceUid(uid);

    int id;
    auto oldIt = m_uid2IdMap.find(uid);
    if (oldIt != m_uid2IdMap.end()) {
        id = oldIt->second;
        auto oldFdIt = m_id2fdMap.find(id);
        if (oldFdIt != m_id2fdMap.end()) {
            int oldFd = oldFdIt->second;
            if (oldFd != fd) {
                m_fd2DeviceMap.erase(oldFd);
                m_fd2Buffer.erase(oldFd);
                close(oldFd);
            }
        }
    } else {
        id = ++m_deviceCnt;
        m_uid2IdMap[uid] = id;
    }
    device.setId(id);

    for (const std::string& maskedUid : dbMgr->queryMasksByClient(uid)) {
        device.addMask(maskedUid);
    }

    m_fd2DeviceMap[fd] = device;
    m_id2fdMap[id] = fd;

    if (!dbMgr->deviceExists(uid)) {
        dbMgr->insertDevice(uid, name, group, "client");
    }

    struct json_object* ack = json_object_new_object();
    json_object_object_add(ack, "source", json_object_new_string("server"));
    json_object_object_add(ack, "cmd",    json_object_new_string("register_ack"));
    json_object_object_add(ack, "seq",    json_object_new_int(0));

    struct json_object* ackParams = json_object_new_object();
    json_object_object_add(ackParams, "code",      json_object_new_int(0));
    json_object_object_add(ackParams, "device_id", json_object_new_int(id));
    json_object_object_add(ackParams, "msg",       json_object_new_string("ok"));
    json_object_object_add(ack, "params", ackParams);

    sendJson(fd, ack);
    json_object_put(ack);
    logger->logMsg(DEBUG, "send register ack to client", true);
}

void EpollManager::handleClientFetchDevices(int fd) {
    struct json_object* result = json_object_new_object();
    json_object_object_add(result, "source", json_object_new_string("server"));
    json_object_object_add(result, "cmd",    json_object_new_string("fetch_devices_ack"));
    json_object_object_add(result, "seq",    json_object_new_int(0));

    struct json_object* params = json_object_new_object();
    struct json_object* deviceList = json_object_new_array();

    // 从数据库拉所有 embedded 设备
    std::vector<DeviceRecord> allDevices = dbMgr->queryAllByType("embedded");

    // 构建 uid → fd 的快速查找（只看在线的 embedded）
    std::unordered_map<std::string, int> onlineUidMap;
    for (auto& p : m_fd2DeviceMap) {
        if (p.second.type() == EMBEDDED) {
            onlineUidMap[p.second.deviceUid()] = p.first;
        }
    }
    std::set<std::string> maskSet;
    auto clientIt = m_fd2DeviceMap.find(fd);
    if (clientIt != m_fd2DeviceMap.end() && clientIt->second.type() == CLIENT) {
        maskSet = clientIt->second.maskUidList();
    } else {
        logger->logMsg(WARNING, "fetch_devices from unregistered client", true);
    }

    for (auto& rec : allDevices) {
        if (maskSet.find(rec.uid) != maskSet.end()) {
            continue;
        }
        struct json_object* deviceObj = json_object_new_object();
        json_object_object_add(deviceObj, "device_uid", json_object_new_string(rec.uid.c_str()));
        json_object_object_add(deviceObj, "name",       json_object_new_string(rec.name.c_str()));
        json_object_object_add(deviceObj, "group",      json_object_new_string(rec.group.c_str()));

        auto onlineIt = onlineUidMap.find(rec.uid);
        if (onlineIt != onlineUidMap.end()) {
            // 在线：附带实时信息
            int onlineFd = onlineIt->second;
            DeviceInfo& device = m_fd2DeviceMap[onlineFd];
            json_object_object_add(deviceObj, "id",          json_object_new_int(device.id()));
            json_object_object_add(deviceObj, "version",     json_object_new_string(device.version().c_str()));
            json_object_object_add(deviceObj, "temperature", json_object_new_string(device.temperature().c_str()));
            json_object_object_add(deviceObj, "mem_usage",   json_object_new_int(device.memUsage()));
            json_object_object_add(deviceObj, "disk_free_mb",json_object_new_int(device.diskFreeMb()));
            json_object_object_add(deviceObj, "online",      json_object_new_boolean(1));
        } else {
            // 离线：只有基本信息
            json_object_object_add(deviceObj, "id",          json_object_new_int(-1));
            json_object_object_add(deviceObj, "version",     json_object_new_string(""));
            json_object_object_add(deviceObj, "temperature", json_object_new_string(""));
            json_object_object_add(deviceObj, "mem_usage",   json_object_new_int(0));
            json_object_object_add(deviceObj, "disk_free_mb",json_object_new_int(0));
            json_object_object_add(deviceObj, "online",      json_object_new_boolean(0));
        }

        json_object_array_add(deviceList, deviceObj);
    }

    json_object_object_add(params, "devices", deviceList);
    json_object_object_add(result, "params", params);

    sendJson(fd, result);
    json_object_put(result);
    logger->logMsg(DEBUG, "send device list to client", true);
}

void EpollManager::handleClientHeartbeat(int fd, struct json_object* root) {
    struct json_object* idObj = NULL;
    json_object_object_get_ex(root, "device_id", &idObj);
    if (!idObj) {
        logger->logMsg(ERROR, "client do not have id!", true);
        return;
    }

    int id = json_object_get_int(idObj);
    std::string str = "get heartbeat from client: " + std::to_string(id);
    logger->logMsg(DEBUG, str.c_str(), true);

    auto it = m_fd2DeviceMap.find(fd);
    if (it != m_fd2DeviceMap.end()) {
        it->second.updateLastUploadTime();
    } else {
        logger->logMsg(WARNING, "heartbeat from unknown client", true);
    }
}

void EpollManager::handleClientRequestUpdateEmbedded(int fd, struct json_object* paramsObj) {
    if (!paramsObj) {
        logger->logMsg(ERROR, "request update embedded without params", true);
        return;
    }

    struct json_object* uidObj = NULL, *groupObj = NULL, *nameObj = NULL;
    json_object_object_get_ex(paramsObj, "device_uid", &uidObj);
    json_object_object_get_ex(paramsObj, "group", &groupObj);
    json_object_object_get_ex(paramsObj, "name", &nameObj);

    const char* uid = uidObj ? json_object_get_string(uidObj) : "unknown";
    const char* group = groupObj ? json_object_get_string(groupObj) : "unknown";
    const char* name = nameObj ? json_object_get_string(nameObj) : "unknown";

    if (strcmp(uid, "unknown") == 0 || strcmp(group, "unknown") == 0 
            || strcmp(name, "unknown") == 0) {
        logger->logMsg(ERROR, "client params error", true);
        return;
    }

    struct json_object* update = json_object_new_object();
    json_object_object_add(update, "source", json_object_new_string("server"));
    json_object_object_add(update, "cmd", json_object_new_string("update_embedded_info"));
    json_object_object_add(update, "seq", json_object_new_int(0));

    struct json_object* updateParam = json_object_new_object();
    json_object_object_add(updateParam, "device_uid", json_object_new_string(uid));
    json_object_object_add(updateParam, "sender", json_object_new_int(fd));
    json_object_object_add(updateParam, "group", json_object_new_string(group));
    json_object_object_add(updateParam, "name", json_object_new_string(name));
    json_object_object_add(update, "params", updateParam);

    int id = m_uid2IdMap[uid];
    int embeddedFd = m_id2fdMap[id];
    sendJson(embeddedFd, update);
    json_object_put(update);
    logger->logMsg(DEBUG, "send update embedded info to embedded...", true);
}

void EpollManager::handleClientRequestFileList(int fd, struct json_object* paramsObj) {
    logger->logMsg(DEBUG, "handle client request filelist", true);
    if (!paramsObj) {
        logger->logMsg(ERROR, "request filelist without params", true);
        return;
    }

    const std::string uploadDir = "/var/www/uploads";
    std::vector<FileEntry> files;

    DIR* dir = opendir(uploadDir.c_str());
    if (!dir) {
        logger->logMsg(ERROR, "failed to open upload dir: " + uploadDir, true);
    } else {
        struct dirent* ent = nullptr;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_name[0] == '.')
                continue;

            const std::string fullPath = uploadDir + "/" + ent->d_name;

            struct stat st;
            if (stat(fullPath.c_str(), &st) != 0)
                continue;
            if (!S_ISREG(st.st_mode))
                continue;

            FileEntry entry;
            entry.filePath = fullPath;
            entry.fileName = ent->d_name;
            entry.fileSize = static_cast<long long>(st.st_size);
            files.push_back(entry);
        }
        closedir(dir);
    }

    struct json_object* result = json_object_new_object();
    json_object_object_add(result, "source", json_object_new_string("server"));
    json_object_object_add(result, "cmd",    json_object_new_string("request_filelist_ack"));
    json_object_object_add(result, "seq",    json_object_new_int(0));

    struct json_object* params = json_object_new_object();
    struct json_object* fileList = json_object_new_array();

    for (const FileEntry& f : files) {
        struct json_object* fileObj = json_object_new_object();

        const std::string serverPath = "/uploads/" + f.fileName;
        json_object_object_add(fileObj, "path", json_object_new_string(serverPath.c_str()));
        json_object_object_add(fileObj, "name", json_object_new_string(f.fileName.c_str()));
        json_object_object_add(fileObj, "size", json_object_new_int64(f.fileSize));

        json_object_array_add(fileList, fileObj);
    }

    json_object_object_add(params, "files", fileList);
    json_object_object_add(params, "count", json_object_new_int(static_cast<int>(files.size())));
    json_object_object_add(result, "params", params);

    sendJson(fd, result);
    json_object_put(result);

    logger->logMsg(DEBUG, "send file list to client, count=" + std::to_string(files.size()), true);
}

void EpollManager::handleClientRequestFirmwareList(int fd, struct json_object* paramsObj) {
    logger->logMsg(DEBUG, "handle client request firmware list", true);

    auto clientIt = m_fd2DeviceMap.find(fd);
    if (clientIt == m_fd2DeviceMap.end() || clientIt->second.type() != CLIENT) {
        logger->logMsg(ERROR, "firmware list request from unregistered client", true);
        return;
    }

    std::vector<FirmwareEntry> firmwares;
    DIR* dir = opendir(FIRMWARE_DIR);
    if (!dir) {
        logger->logMsg(ERROR, std::string("failed to open firmware dir: ") + FIRMWARE_DIR, true);
    } else {
        struct dirent* ent = nullptr;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_name[0] == '.') {
                continue;
            }
            if (!isSafeBasename(ent->d_name) || !isFirmwareArchive(ent->d_name)) {
                continue;
            }

            const std::string fullPath = std::string(FIRMWARE_DIR) + "/" + ent->d_name;
            struct stat st;
            if (stat(fullPath.c_str(), &st) != 0) {
                continue;
            }
            if (!S_ISREG(st.st_mode)) {
                continue;
            }

            firmwares.push_back(inspectFirmwareArchive(
                fullPath, ent->d_name, static_cast<long long>(st.st_size)));
        }
        closedir(dir);
    }

    struct json_object* result = json_object_new_object();
    json_object_object_add(result, "source", json_object_new_string("server"));
    json_object_object_add(result, "cmd",    json_object_new_string("request_firmware_list_ack"));
    json_object_object_add(result, "seq",    json_object_new_int(0));

    struct json_object* params = json_object_new_object();
    struct json_object* firmwareList = json_object_new_array();

    for (const FirmwareEntry& fw : firmwares) {
        json_object_array_add(firmwareList, firmwareEntryToJson(fw));
    }

    json_object_object_add(params, "firmwares", firmwareList);
    json_object_object_add(params, "count", json_object_new_int(static_cast<int>(firmwares.size())));
    json_object_object_add(result, "params", params);

    sendJson(fd, result);
    json_object_put(result);

    logger->logMsg(DEBUG, "send firmware list to client, count="
                   + std::to_string(firmwares.size()), true);
}

void EpollManager::handleClientMaskDevice(int fd, struct json_object* paramsObj) {
    logger->logMsg(DEBUG, "handle client mask device", true);
    if (!paramsObj) {
        logger->logMsg(ERROR, "request mask device without params", true);
        return;
    }

    struct json_object* uidObj = NULL;
    json_object_object_get_ex(paramsObj, "device_uid", &uidObj);
    if (!uidObj) {
        logger->logMsg(ERROR, "mask device without device_uid", true);
        return;
    }
    const char* deviceUid = json_object_get_string(uidObj);

    auto it = m_fd2DeviceMap.find(fd);
    if (it == m_fd2DeviceMap.end() || it->second.type() != CLIENT) {
        logger->logMsg(ERROR, "mask request from unregistered client", true);
        return;
    }

    const std::string& clientUid = it->second.deviceUid();
    dbMgr->insertMask(clientUid, deviceUid);
    it->second.addMask(deviceUid);

    struct json_object* ack = json_object_new_object();
    json_object_object_add(ack, "source", json_object_new_string("server"));
    json_object_object_add(ack, "cmd",    json_object_new_string("mask_device_ack"));
    json_object_object_add(ack, "seq",    json_object_new_int(0));

    struct json_object* ackParams = json_object_new_object();
    json_object_object_add(ackParams, "code",       json_object_new_int(0));
    json_object_object_add(ackParams, "device_uid", json_object_new_string(deviceUid));
    json_object_object_add(ackParams, "msg",        json_object_new_string("ok"));
    json_object_object_add(ack, "params", ackParams);

    sendJson(fd, ack);
    json_object_put(ack);
    logger->logMsg(DEBUG, "mask device persisted", true);
}

bool EpollManager::pushResourcesToEmbedded(const std::string& uid,
                                           const std::vector<std::string>& relativePaths) {
    if (relativePaths.empty()) {
        return false;
    }

    auto uidIt = m_uid2IdMap.find(uid);
    if (uidIt == m_uid2IdMap.end()) {
        logger->logMsg(WARNING, std::string("push resources: unknown device uid: ") + uid, true);
        return false;
    }
    auto fdIt = m_id2fdMap.find(uidIt->second);
    if (fdIt == m_id2fdMap.end()) {
        logger->logMsg(WARNING, std::string("push resources: device offline: ") + uid, true);
        return false;
    }
    int embFd = fdIt->second;
    auto devIt = m_fd2DeviceMap.find(embFd);
    if (devIt == m_fd2DeviceMap.end() || devIt->second.type() != EMBEDDED) {
        return false;
    }

    const std::string urlPrefix = std::string("http://") + IP;
    struct json_object* push = json_object_new_object();
    json_object_object_add(push, "source", json_object_new_string("server"));
    json_object_object_add(push, "cmd",    json_object_new_string("push_resources_to_download"));
    json_object_object_add(push, "seq",    json_object_new_int(0));

    struct json_object* pushParams = json_object_new_object();
    struct json_object* urlList = json_object_new_array();
    for (const std::string& path : relativePaths) {
        json_object_array_add(urlList, json_object_new_string((urlPrefix + path).c_str()));
    }
    json_object_object_add(pushParams, "paths", urlList);
    json_object_object_add(push, "params", pushParams);

    sendJson(embFd, push);
    json_object_put(push);
    logger->logMsg(DEBUG, std::string("push resources to embedded: ") + uid, true);
    return true;
}

void EpollManager::handleClientRequestPushContentToEmbedded(int fd, struct json_object* paramsObj) {
    logger->logMsg(DEBUG, "handle client request push content to embedded", true);
    if (!paramsObj) {
        logger->logMsg(ERROR, "request push content without params", true);
        return;
    }

    auto clientIt = m_fd2DeviceMap.find(fd);
    if (clientIt == m_fd2DeviceMap.end() || clientIt->second.type() != CLIENT) {
        logger->logMsg(ERROR, "push content request from unregistered client", true);
        return;
    }

    struct json_object* pathsObj = NULL;
    json_object_object_get_ex(paramsObj, "paths", &pathsObj);
    if (!pathsObj || json_object_get_type(pathsObj) != json_type_array) {
        logger->logMsg(ERROR, "push content without paths array", true);
        return;
    }

    std::vector<std::string> paths;
    int pathLen = json_object_array_length(pathsObj);
    for (int i = 0; i < pathLen; i++) {
        struct json_object* item = json_object_array_get_idx(pathsObj, i);
        if (!item || json_object_get_type(item) != json_type_string) {
            continue;
        }
        const char* path = json_object_get_string(item);
        if (!path || path[0] == '\0') {
            continue;
        }
        paths.push_back(path);
    }
    if (paths.empty()) {
        logger->logMsg(ERROR, "push content paths empty", true);
        return;
    }

    struct json_object* uidsObj = NULL;
    json_object_object_get_ex(paramsObj, "device_uids", &uidsObj);
    if (!uidsObj || json_object_get_type(uidsObj) != json_type_array) {
        logger->logMsg(ERROR, "push content without device_uids array", true);
        return;
    }
    if (json_object_array_length(uidsObj) == 0) {
        logger->logMsg(ERROR, "push content device_uids empty", true);
        return;
    }

    int uidLen = json_object_array_length(uidsObj);
    for (int i = 0; i < uidLen; i++) {
        struct json_object* item = json_object_array_get_idx(uidsObj, i);
        if (!item || json_object_get_type(item) != json_type_string) {
            continue;
        }
        const char* uid = json_object_get_string(item);
        if (!uid || uid[0] == '\0') {
            continue;
        }
        pushResourcesToEmbedded(uid, paths);
    }
}

void EpollManager::handleClientRequestScreenshot(int fd, struct json_object* paramsObj) {
    logger->logMsg(DEBUG, "handle client request screenshot", true);
    if (!paramsObj) {
        logger->logMsg(ERROR, "request screenshot without params", true);
        return;
    }

    auto clientIt = m_fd2DeviceMap.find(fd);
    if (clientIt == m_fd2DeviceMap.end() || clientIt->second.type() != CLIENT) {
        logger->logMsg(ERROR, "screenshot request from unregistered client", true);
        return;
    }

    struct json_object* uidObj = NULL;
    json_object_object_get_ex(paramsObj, "device_uid", &uidObj);
    if (!uidObj) {
        logger->logMsg(ERROR, "request screenshot without device_uid", true);
        return;
    }
    const char* deviceUid = json_object_get_string(uidObj);
    if (!deviceUid || deviceUid[0] == '\0') {
        logger->logMsg(ERROR, "request screenshot device_uid empty", true);
        return;
    }

    logger->logMsg(DEBUG, std::string("screenshot target device_uid: ") + deviceUid, true);

    auto uidIt = m_uid2IdMap.find(deviceUid);
    if (uidIt == m_uid2IdMap.end()) {
        logger->logMsg(WARNING, std::string("screenshot: unknown device uid: ") + deviceUid, true);
        return;
    }
    auto fdIt = m_id2fdMap.find(uidIt->second);
    if (fdIt == m_id2fdMap.end()) {
        logger->logMsg(WARNING, std::string("screenshot: device offline: ") + deviceUid, true);
        return;
    }

    struct json_object* req = json_object_new_object();
    json_object_object_add(req, "source", json_object_new_string("server"));
    json_object_object_add(req, "cmd",    json_object_new_string("request_screenshot"));
    json_object_object_add(req, "seq",    json_object_new_int(0));

    struct json_object* reqParams = json_object_new_object();
    json_object_object_add(reqParams, "device_id", json_object_new_int(clientIt->second.id()));
    json_object_object_add(req, "params", reqParams);

    sendJson(fdIt->second, req);
    json_object_put(req);
    logger->logMsg(DEBUG, std::string("send screenshot request to embedded: ") + deviceUid, true);
}

void EpollManager::handleEmbeddedScreenshotData(int fd, struct json_object* paramsObj) {
    (void)fd;
    logger->logMsg(DEBUG, "handle embedded screenshot data", true);
    if (!paramsObj) {
        logger->logMsg(ERROR, "screenshot_data without params", true);
        return;
    }

    struct json_object* clientIdObj = NULL;
    struct json_object* pathObj = NULL;
    json_object_object_get_ex(paramsObj, "device_id", &clientIdObj);
    json_object_object_get_ex(paramsObj, "path", &pathObj);

    if (!clientIdObj) {
        logger->logMsg(ERROR, "screenshot_data without device_id", true);
        return;
    }

    int clientId = json_object_get_int(clientIdObj);
    auto fdIt = m_id2fdMap.find(clientId);
    if (fdIt == m_id2fdMap.end()) {
        logger->logMsg(WARNING, std::string("screenshot_data: client offline, id=") + std::to_string(clientId), true);
        return;
    }

    int clientFd = fdIt->second;
    auto clientIt = m_fd2DeviceMap.find(clientFd);
    if (clientIt == m_fd2DeviceMap.end() || clientIt->second.type() != CLIENT) {
        logger->logMsg(ERROR, "screenshot_data: device_id is not a client", true);
        return;
    }

    const char* path = pathObj ? json_object_get_string(pathObj) : "";
    if (!path || path[0] == '\0') {
        logger->logMsg(WARNING, "screenshot_data without path", true);
        return;
    }

    struct json_object* ack = json_object_new_object();
    json_object_object_add(ack, "source", json_object_new_string("server"));
    json_object_object_add(ack, "cmd",    json_object_new_string("request_screenshot_ack"));
    json_object_object_add(ack, "seq",    json_object_new_int(0));

    struct json_object* ackParams = json_object_new_object();
    json_object_object_add(ackParams, "path", json_object_new_string(path));
    json_object_object_add(ack, "params", ackParams);

    sendJson(clientFd, ack);
    json_object_put(ack);
    logger->logMsg(DEBUG, std::string("send screenshot ack to client id=") + std::to_string(clientId)
                   + " path=" + path, true);
}

void EpollManager::handleEmbeddedOtaUpdateAck(int fd, struct json_object* paramsObj) {
    (void)fd;
    logger->logMsg(DEBUG, "handle embedded ota_update_ack", true);
    if (!paramsObj) {
        logger->logMsg(ERROR, "ota_update_ack without params", true);
        return;
    }

    struct json_object* clientUidObj = NULL;
    json_object_object_get_ex(paramsObj, "device_uid", &clientUidObj);
    if (!clientUidObj || json_object_get_type(clientUidObj) != json_type_string) {
        logger->logMsg(ERROR, "ota_update_ack without device_uid", true);
        return;
    }
    const char* clientUid = json_object_get_string(clientUidObj);
    if (!clientUid || clientUid[0] == '\0') {
        logger->logMsg(ERROR, "ota_update_ack device_uid empty", true);
        return;
    }

    struct json_object* resultObj = NULL;
    json_object_object_get_ex(paramsObj, "result", &resultObj);
    int result = (resultObj && json_object_get_int(resultObj) == 1) ? 1 : 0;

    auto clientUidIt = m_uid2IdMap.find(clientUid);
    if (clientUidIt == m_uid2IdMap.end()) {
        logger->logMsg(WARNING, std::string("ota_update_ack: client offline or unknown: ") + clientUid, true);
        return;
    }
    auto clientFdIt = m_id2fdMap.find(clientUidIt->second);
    if (clientFdIt == m_id2fdMap.end()) {
        logger->logMsg(WARNING, std::string("ota_update_ack: client offline: ") + clientUid, true);
        return;
    }
    int clientFd = clientFdIt->second;
    auto clientDevIt = m_fd2DeviceMap.find(clientFd);
    if (clientDevIt == m_fd2DeviceMap.end() || clientDevIt->second.type() != CLIENT) {
        logger->logMsg(ERROR, "ota_update_ack: device_uid is not a client", true);
        return;
    }

    struct json_object* ack = json_object_new_object();
    json_object_object_add(ack, "source", json_object_new_string("server"));
    json_object_object_add(ack, "cmd",    json_object_new_string("ota_update_ack"));
    json_object_object_add(ack, "seq",    json_object_new_int(0));

    struct json_object* ackParams = json_object_new_object();
    json_object_object_add(ackParams, "result", json_object_new_int(result));
    json_object_object_add(ack, "params", ackParams);

    sendJson(clientFd, ack);
    json_object_put(ack);
    logger->logMsg(DEBUG, std::string("forward ota_update_ack to client=") + clientUid
                   + " result=" + std::to_string(result), true);
}

void EpollManager::handleClientRequestSchedulePush(int fd, struct json_object* paramsObj) {
    logger->logMsg(DEBUG, "handle client request schedule push", true);
    if (!paramsObj) {
        logger->logMsg(ERROR, "request schedule push without params", true);
        return;
    }

    auto clientIt = m_fd2DeviceMap.find(fd);
    if (clientIt == m_fd2DeviceMap.end() || clientIt->second.type() != CLIENT) {
        logger->logMsg(ERROR, "schedule push request from unregistered client", true);
        return;
    }

    struct json_object* pathsObj = NULL;
    json_object_object_get_ex(paramsObj, "paths", &pathsObj);
    if (!pathsObj || json_object_get_type(pathsObj) != json_type_array) {
        logger->logMsg(ERROR, "schedule push without paths array", true);
        return;
    }

    // get all paths
    std::vector<std::string> paths;
    int pathLen = json_object_array_length(pathsObj);
    for (int i = 0; i < pathLen; i++) {
        struct json_object* item = json_object_array_get_idx(pathsObj, i);
        if (!item || json_object_get_type(item) != json_type_string) {
            continue;
        }
        const char* path = json_object_get_string(item);
        if (!path || path[0] == '\0') {
            continue;
        }
        paths.push_back(path);
    }
    if (paths.empty()) {
        logger->logMsg(ERROR, "schedule push paths empty", true);
        return;
    }

    struct json_object* uidsObj = NULL;
    json_object_object_get_ex(paramsObj, "device_uids", &uidsObj);
    if (!uidsObj || json_object_get_type(uidsObj) != json_type_array) {
        logger->logMsg(ERROR, "schedule push without device_uids array", true);
        return;
    }

    // get all uids
    std::vector<std::string> deviceUids;
    int uidLen = json_object_array_length(uidsObj);
    for (int i = 0; i < uidLen; i++) {
        struct json_object* item = json_object_array_get_idx(uidsObj, i);
        if (!item || json_object_get_type(item) != json_type_string) {
            continue;
        }
        const char* uid = json_object_get_string(item);
        if (!uid || uid[0] == '\0') {
            continue;
        }
        deviceUids.push_back(uid);
    }
    if (deviceUids.empty()) {
        logger->logMsg(ERROR, "schedule push device_uids empty", true);
        return;
    }

    struct json_object* dateObj = NULL;
    struct json_object* timeObj = NULL;
    struct json_object* durationObj = NULL;
    json_object_object_get_ex(paramsObj, "schedule_date", &dateObj);
    json_object_object_get_ex(paramsObj, "schedule_time", &timeObj);
    json_object_object_get_ex(paramsObj, "duration_sec", &durationObj);

    if (!dateObj || json_object_get_type(dateObj) != json_type_string) {
        logger->logMsg(ERROR, "schedule push without schedule_date", true);
        return;
    }
    if (!timeObj || json_object_get_type(timeObj) != json_type_string) {
        logger->logMsg(ERROR, "schedule push without schedule_time", true);
        return;
    }
    if (!durationObj || json_object_get_type(durationObj) != json_type_int) {
        logger->logMsg(ERROR, "schedule push without duration_sec", true);
        return;
    }

    const char* scheduleDate = json_object_get_string(dateObj);
    const char* scheduleTime = json_object_get_string(timeObj);
    int durationSec = json_object_get_int(durationObj);

    if (!scheduleDate || scheduleDate[0] == '\0' ||
        !scheduleTime || scheduleTime[0] == '\0') {
        logger->logMsg(ERROR, "schedule push date or time empty", true);
        return;
    }
    if (durationSec <= 0) {
        logger->logMsg(ERROR, "schedule push duration_sec invalid", true);
        return;
    }

    logger->logMsg(DEBUG, std::string("schedule push date=") + scheduleDate
                   + " time=" + scheduleTime
                   + " duration_sec=" + std::to_string(durationSec), true);

    const std::string urlPrefix = std::string("http://") + IP;
    std::vector<std::string> urls;
    for (const std::string& path : paths) {
        urls.push_back(urlPrefix + path);
    }

    time_t now = time(NULL);
    for (const std::string& uid : deviceUids) {
        auto uidIt = m_uid2IdMap.find(uid);
        if (uidIt == m_uid2IdMap.end()) {
            logger->logMsg(WARNING, std::string("schedule push: unknown uid: ") + uid, true);
            continue;
        }
        auto fdIt = m_id2fdMap.find(uidIt->second);
        if (fdIt == m_id2fdMap.end()) {
            logger->logMsg(WARNING, std::string("schedule push: device offline: ") + uid, true);
            continue;
        }
        auto devIt = m_fd2DeviceMap.find(fdIt->second);
        if (devIt == m_fd2DeviceMap.end() || devIt->second.type() != EMBEDDED) {
            logger->logMsg(WARNING, std::string("schedule push: not embedded device: ") + uid, true);
            continue;
        }

        time_t devTs = devIt->second.deviceTimestamp();  // device localtime
        time_t diff = (now >= devTs) ? (now - devTs) : (devTs - now);
        bool clockTrusted = (devTs > 0 && diff <= 60);  // is clock trusted

        if (clockTrusted) {
            logger->logMsg(DEBUG, std::string("schedule push clock trusted: ") + uid, true);

            struct json_object* push = json_object_new_object();
            json_object_object_add(push, "source", json_object_new_string("server"));
            json_object_object_add(push, "cmd",    json_object_new_string("push_schedule_playlist"));
            json_object_object_add(push, "seq",    json_object_new_int(0));

            struct json_object* pushParams = json_object_new_object();
            struct json_object* pathArr = json_object_new_array();
            for (const std::string& url : urls) {
                json_object_array_add(pathArr, json_object_new_string(url.c_str()));
            }
            json_object_object_add(pushParams, "paths", pathArr);
            json_object_object_add(pushParams, "schedule_date", json_object_new_string(scheduleDate));
            json_object_object_add(pushParams, "schedule_time", json_object_new_string(scheduleTime));
            json_object_object_add(pushParams, "duration_sec", json_object_new_int(durationSec));
            json_object_object_add(push, "params", pushParams);

            sendJson(fdIt->second, push);
            json_object_put(push);
            logger->logMsg(DEBUG, std::string("send schedule playlist to embedded: ") + uid, true);
        } else {
            logger->logMsg(DEBUG, std::string("schedule push clock untrusted: ") + uid
                           + " diff=" + std::to_string(diff) + "s", true);

            time_t triggerAt = ScheduleMgr::parseTriggerAt(scheduleDate, scheduleTime);
            if (triggerAt <= 0) {
                logger->logMsg(ERROR, std::string("schedule push invalid trigger time: ") + uid, true);
                continue;
            }

            ScheduleTask task;
            task.deviceUid    = uid;
            task.scheduleDate = scheduleDate;
            task.scheduleTime = scheduleTime;
            task.durationSec  = durationSec;
            task.triggerAt    = triggerAt;
            task.paths        = paths;
            task.enabled      = true;

            int taskId = ScheduleMgr::getInstance().addTask(task);
            if (taskId <= 0) {
                logger->logMsg(ERROR, std::string("schedule push addTask failed: ") + uid, true);
            } else {
                logger->logMsg(DEBUG, std::string("schedule push task queued id=")
                               + std::to_string(taskId) + " uid=" + uid, true);
            }
        }
    }
}

void EpollManager::handleClientRequestOTAUpdate(int fd, struct json_object* paramsObj) {
    logger->logMsg(DEBUG, "handle client request OTA update", true);
    if (!paramsObj) {
        logger->logMsg(ERROR, "request OTA update without params", true);
        return;
    }

    auto clientIt = m_fd2DeviceMap.find(fd);
    if (clientIt == m_fd2DeviceMap.end() || clientIt->second.type() != CLIENT) {
        logger->logMsg(ERROR, "OTA update request from unregistered client", true);
        return;
    }

    struct json_object* uidsObj = NULL;
    json_object_object_get_ex(paramsObj, "device_uids", &uidsObj);
    if (!uidsObj || json_object_get_type(uidsObj) != json_type_array) {
        logger->logMsg(ERROR, "OTA update without device_uids array", true);
        return;
    }

    std::vector<std::string> deviceUids;
    int uidLen = json_object_array_length(uidsObj);
    for (int i = 0; i < uidLen; i++) {
        struct json_object* item = json_object_array_get_idx(uidsObj, i);
        if (!item || json_object_get_type(item) != json_type_string) {
            continue;
        }
        const char* uid = json_object_get_string(item);
        if (!uid || uid[0] == '\0') {
            continue;
        }
        deviceUids.push_back(uid);
    }
    if (deviceUids.empty()) {
        logger->logMsg(ERROR, "OTA update device_uids empty", true);
        return;
    }

    struct json_object* clientUidObj = NULL;
    json_object_object_get_ex(paramsObj, "device_uid", &clientUidObj);
    std::string clientUid;
    if (clientUidObj && json_object_get_type(clientUidObj) == json_type_string) {
        clientUid = json_object_get_string(clientUidObj);
    } else {
        clientUid = clientIt->second.deviceUid();
    }
    if (clientUid.empty()) {
        logger->logMsg(ERROR, "OTA update client device_uid empty", true);
        return;
    }

    struct json_object* pathObj = NULL;
    json_object_object_get_ex(paramsObj, "path", &pathObj);
    if (!pathObj || json_object_get_type(pathObj) != json_type_string) {
        logger->logMsg(ERROR, "OTA update without path", true);
        return;
    }
    const char* path = json_object_get_string(pathObj);
    if (!path || path[0] == '\0') {
        logger->logMsg(ERROR, "OTA update path empty", true);
        return;
    }

    const std::string pathStr(path);

    std::string firmwarePath;
    if (!resolveFirmwarePath(pathStr, firmwarePath)) {
        logger->logMsg(ERROR, "OTA firmware not found: " + pathStr, true);
        return;
    }

    std::string firmwareMd5 = computeFileMd5(firmwarePath);
    if (firmwareMd5.empty()) {
        logger->logMsg(ERROR, "OTA firmware md5 compute failed: " + firmwarePath, true);
        return;
    }

    logger->logMsg(DEBUG, "OTA update path=" + pathStr
                   + " md5=" + firmwareMd5
                   + " device_count=" + std::to_string(deviceUids.size()), true);

    for (const std::string& uid : deviceUids) {
        auto uidIt = m_uid2IdMap.find(uid);
        if (uidIt == m_uid2IdMap.end()) {
            logger->logMsg(WARNING, std::string("OTA update: unknown uid: ") + uid, true);
            continue;
        }
        auto fdIt = m_id2fdMap.find(uidIt->second);
        if (fdIt == m_id2fdMap.end()) {
            logger->logMsg(WARNING, std::string("OTA update: device offline: ") + uid, true);
            continue;
        }
        int embFd = fdIt->second;
        auto devIt = m_fd2DeviceMap.find(embFd);
        if (devIt == m_fd2DeviceMap.end() || devIt->second.type() != EMBEDDED) {
            logger->logMsg(WARNING, std::string("OTA update: not embedded device: ") + uid, true);
            continue;
        }

        struct json_object* ota = json_object_new_object();
        json_object_object_add(ota, "source", json_object_new_string("server"));
        json_object_object_add(ota, "cmd",    json_object_new_string("ota_update"));
        json_object_object_add(ota, "seq",    json_object_new_int(0));

        struct json_object* otaParams = json_object_new_object();
        json_object_object_add(otaParams, "path",       json_object_new_string(pathStr.c_str()));
        json_object_object_add(otaParams, "md5",        json_object_new_string(firmwareMd5.c_str()));
        json_object_object_add(otaParams, "device_uid", json_object_new_string(clientUid.c_str()));
        json_object_object_add(ota, "params", otaParams);

        sendJson(embFd, ota);
        json_object_put(ota);
        logger->logMsg(DEBUG, std::string("send ota_update to embedded: ") + uid, true);
    }
}

void EpollManager::handleClientRequestCheckFirmware(int fd, struct json_object* paramsObj) {
    logger->logMsg(DEBUG, "handle client request check firmware", true);

    auto sendCheckAck = [&](int result) {
        struct json_object* ack = json_object_new_object();
        json_object_object_add(ack, "source", json_object_new_string("server"));
        json_object_object_add(ack, "cmd",    json_object_new_string("check_firmware_ack"));
        json_object_object_add(ack, "seq",    json_object_new_int(0));

        struct json_object* ackParams = json_object_new_object();
        json_object_object_add(ackParams, "result", json_object_new_int(result));
        json_object_object_add(ack, "params", ackParams);

        sendJson(fd, ack);
        json_object_put(ack);
    };

    if (!paramsObj) {
        logger->logMsg(ERROR, "check firmware without params", true);
        sendCheckAck(0);
        return;
    }

    auto clientIt = m_fd2DeviceMap.find(fd);
    if (clientIt == m_fd2DeviceMap.end() || clientIt->second.type() != CLIENT) {
        logger->logMsg(ERROR, "check firmware request from unregistered client", true);
        sendCheckAck(0);
        return;
    }

    struct json_object* pathObj = NULL;
    json_object_object_get_ex(paramsObj, "path", &pathObj);
    if (!pathObj || json_object_get_type(pathObj) != json_type_string) {
        logger->logMsg(ERROR, "check firmware without path", true);
        sendCheckAck(0);
        return;
    }
    const char* path = json_object_get_string(pathObj);
    if (!path || path[0] == '\0') {
        logger->logMsg(ERROR, "check firmware path empty", true);
        sendCheckAck(0);
        return;
    }

    struct json_object* md5Obj = NULL;
    json_object_object_get_ex(paramsObj, "md5", &md5Obj);
    if (!md5Obj || json_object_get_type(md5Obj) != json_type_string) {
        logger->logMsg(ERROR, "check firmware without md5", true);
        sendCheckAck(0);
        return;
    }
    const char* expectMd5 = json_object_get_string(md5Obj);
    if (!expectMd5 || !isHexMd5(expectMd5)) {
        logger->logMsg(ERROR, "check firmware invalid md5", true);
        sendCheckAck(0);
        return;
    }

    std::string firmwarePath;
    if (!resolveFirmwarePath(path, firmwarePath)) {
        logger->logMsg(ERROR, std::string("check firmware file not found: ") + path, true);
        sendCheckAck(0);
        return;
    }

    std::string actualMd5 = computeFileMd5(firmwarePath);
    if (actualMd5.empty()) {
        logger->logMsg(ERROR, "check firmware md5 compute failed: " + firmwarePath, true);
        sendCheckAck(0);
        return;
    }

    int result = md5EqualsIgnoreCase(actualMd5, expectMd5) ? 1 : 0;
    logger->logMsg(DEBUG, std::string("check firmware path=") + path
                   + " expect=" + expectMd5
                   + " actual=" + actualMd5
                   + " result=" + std::to_string(result), true);
    sendCheckAck(result);
}