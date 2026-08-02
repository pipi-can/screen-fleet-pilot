#ifndef __SCHEDULEMGR_H__
#define __SCHEDULEMGR_H__

#include <ctime>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/timerfd.h>   // timerfd_create, timerfd_settime, itimerspec
#include <time.h>          // CLOCK_REALTIME, timespec
#include <fcntl.h>         // TFD_NONBLOCK, TFD_CLOEXEC (部分系统)
#include <unistd.h>

extern "C" {
#include <bits/time.h>
#include <json-c/json.h>
}

struct ScheduleTask {
    int id;
    std::string deviceUid;
    std::string scheduleDate;
    std::string scheduleTime;
    int durationSec;
    time_t triggerAt;
    std::vector<std::string> paths;
    bool enabled;
};

struct ScheduleTaskCompare {
    bool operator()(const ScheduleTask& a, const ScheduleTask& b) const;
};

class ScheduleMgr {
public:
    ScheduleMgr(const ScheduleMgr& other) = delete;
    void operator=(const ScheduleMgr& other) = delete;

    static ScheduleMgr& getInstance();

    void init();
    void shutdown();

    int addTask(const ScheduleTask& task);
    void removeTask(int id);
    void removeTasksByDevice(const std::string& deviceUid);

    void onTimerExpired();

    int timerFd() const;

    static time_t parseTriggerAt(const std::string& scheduleDate,
                                 const std::string& scheduleTime);

    static std::vector<std::string> parsePaths(const char* pathsJson);

    static std::string encodePaths(const std::vector<std::string>& paths);

private:
    ScheduleMgr();
    ~ScheduleMgr();

    void reloadFromDb();
    void pushHeap(const ScheduleTask& task);
    void updateTimerfd();
    void executeTask(const ScheduleTask& task);
    int allocTaskId();

    std::priority_queue<
        ScheduleTask,
        std::vector<ScheduleTask>,
        ScheduleTaskCompare
    > m_heap;

    std::unordered_map<int, ScheduleTask> m_taskMap;
    int m_timerFd;
    int m_nextId;
};

#endif
