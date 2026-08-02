#include "../includes/schedulemgr.h"
#include "../includes/databasemgr.h"
#include "../includes/epollmgr.h"
#include "../includes/logmgr.h"
#include <cstring>

static LogMgr* logger = &LogMgr::getInstance();

bool ScheduleTaskCompare::operator()(const ScheduleTask& a, const ScheduleTask& b) const {
    return a.triggerAt > b.triggerAt;
}

ScheduleMgr& ScheduleMgr::getInstance() {
    static ScheduleMgr instance;
    return instance;
}

ScheduleMgr::ScheduleMgr()
    : m_timerFd(-1)
    , m_nextId(1) {
}

ScheduleMgr::~ScheduleMgr() {
}

void ScheduleMgr::init() {
    if (m_timerFd >= 0) {
        return ;
    }
    m_timerFd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (m_timerFd < 0) { 
        perror("[server]: timerfd_create failed");
        return ;
    }

    reloadFromDb();
    updateTimerfd();
}

void ScheduleMgr::shutdown() {
    if (m_timerFd >= 0) {
        close(m_timerFd);
        m_timerFd = -1;
    }
}

int ScheduleMgr::addTask(const ScheduleTask& task) {
    if (!task.enabled || task.paths.empty() || task.deviceUid.empty() || task.triggerAt <= 0) {
        return -1;
    }

    ScheduleTask copy = task;
    copy.id = DatabaseMgr::getInstance().insertScheduleTask(copy);
    if (copy.id <= 0) {
        return -1;
    }

    bool needUpdate = m_heap.empty() || copy.triggerAt <= m_heap.top().triggerAt;
    m_taskMap[copy.id] = copy;
    pushHeap(copy);
    if (copy.id >= m_nextId) {
        m_nextId = copy.id + 1;
    }
    if (needUpdate) {
        updateTimerfd();
    }
    return copy.id;
}

void ScheduleMgr::removeTask(int id) {
    auto it = m_taskMap.find(id);
    if (it == m_taskMap.end()) {
        return;
    }
    it->second.enabled = false;
    DatabaseMgr::getInstance().disableScheduleTask(id);
}

void ScheduleMgr::removeTasksByDevice(const std::string& deviceUid) {
    for (auto& p : m_taskMap) {
        if (p.second.deviceUid == deviceUid && p.second.enabled) {
            p.second.enabled = false;
            DatabaseMgr::getInstance().disableScheduleTask(p.first);
        }
    }
}

void ScheduleMgr::onTimerExpired() {
    uint64_t expirations = 0;
    read(m_timerFd, &expirations, sizeof(expirations));

    time_t now = time(NULL);
    while (!m_heap.empty() && m_heap.top().triggerAt <= now) {
        ScheduleTask task = m_heap.top();
        m_heap.pop();
        if (!task.enabled) {
            continue;
        }
        executeTask(task);
    }
    updateTimerfd();
}

int ScheduleMgr::timerFd() const {
    return m_timerFd;
}

time_t ScheduleMgr::parseTriggerAt(const std::string& scheduleDate,
                                   const std::string& scheduleTime) {
    std::string dt = scheduleDate + " " + scheduleTime;
    struct tm tmVal;
    memset(&tmVal, 0, sizeof(tmVal));

    const char* rest = strptime(dt.c_str(), "%Y-%m-%d %H:%M:%S", &tmVal);
    if (!rest) {
        memset(&tmVal, 0, sizeof(tmVal));
        rest = strptime(dt.c_str(), "%Y-%m-%d %H:%M", &tmVal);
    }
    if (!rest) {
        return 0;
    }

    tmVal.tm_isdst = -1;
    return mktime(&tmVal);
}

void ScheduleMgr::reloadFromDb() {
    m_heap = decltype(m_heap)();
    m_taskMap.clear();
    m_nextId = 1;

    std::vector<ScheduleTask> tasks =
        DatabaseMgr::getInstance().queryPendingScheduleTasks(time(NULL));
    int maxId = 0;
    for (const ScheduleTask& task : tasks) {
        m_taskMap[task.id] = task;
        pushHeap(task);
        if (task.id > maxId) {
            maxId = task.id;
        }
    }
    if (maxId > 0) {
        m_nextId = maxId + 1;
    }
}

void ScheduleMgr::pushHeap(const ScheduleTask& task) {
    if (task.enabled) {
        m_heap.push(task);
    }
}

void ScheduleMgr::updateTimerfd() {
    if (m_timerFd < 0) {
        return;
    }
    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    if (m_heap.empty()) {
        timerfd_settime(m_timerFd, 0, &its, nullptr);
        return;
    }

    time_t next = m_heap.top().triggerAt;
    its.it_value.tv_sec  = next;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec  = 0;
    its.it_interval.tv_nsec = 0;
    if (timerfd_settime(m_timerFd, TFD_TIMER_ABSTIME, &its, nullptr) < 0) {
        perror("[server]: timerfd_settime failed");
    }
}

void ScheduleMgr::executeTask(const ScheduleTask& task) {
    if (!task.enabled || task.paths.empty()) {
        return;
    }

    bool ok = EpollManager::getInstance().pushResourcesToEmbedded(task.deviceUid, task.paths);
    if (!ok) {
        logger->logMsg(WARNING,
            std::string("executeTask failed id=") + std::to_string(task.id)
            + " uid=" + task.deviceUid, true);
        DatabaseMgr::getInstance().disableScheduleTask(task.id);
        m_taskMap.erase(task.id);
        return;
    }

    logger->logMsg(DEBUG,
        std::string("executeTask ok id=") + std::to_string(task.id)
        + " uid=" + task.deviceUid, true);

    DatabaseMgr::getInstance().disableScheduleTask(task.id);
    m_taskMap.erase(task.id);
}

int ScheduleMgr::allocTaskId() {
    return m_nextId++;
}

std::vector<std::string> ScheduleMgr::parsePaths(const char* pathsJson) {
    std::vector<std::string> paths;
    if (!pathsJson || pathsJson[0] == '\0') {
        return paths;
    }

    struct json_object* arr = json_tokener_parse(pathsJson);
    if (!arr || json_object_get_type(arr) != json_type_array) {
        if (arr) {
            json_object_put(arr);
        }
        return paths;
    }

    int len = json_object_array_length(arr);
    for (int i = 0; i < len; i++) {
        struct json_object* item = json_object_array_get_idx(arr, i);
        if (!item || json_object_get_type(item) != json_type_string) {
            continue;
        }
        const char* path = json_object_get_string(item);
        if (path && path[0] != '\0') {
            paths.push_back(path);
        }
    }
    json_object_put(arr);
    return paths;
}

std::string ScheduleMgr::encodePaths(const std::vector<std::string>& paths) {
    struct json_object* arr = json_object_new_array();
    for (const std::string& path : paths) {
        json_object_array_add(arr, json_object_new_string(path.c_str()));
    }
    const char* jsonStr = json_object_to_json_string_ext(arr, JSON_C_TO_STRING_PLAIN);
    std::string result = jsonStr ? jsonStr : "[]";
    json_object_put(arr);
    return result;
}
