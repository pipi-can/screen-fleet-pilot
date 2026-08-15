#ifndef SCHEDULEMGR_H
#define SCHEDULEMGR_H

#include <ctime>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/timerfd.h>

struct ScheduleTask {
    int id = 0;
    std::string deviceUid;
    std::string scheduleDate;
    std::string scheduleTime;
    int durationSec = 0;
    time_t triggerAt = 0;
    std::vector<std::string> paths;
    bool enabled = true;
};

struct ScheduleTaskCompare {
    bool operator()(const ScheduleTask& a, const ScheduleTask& b) const;
};

class ScheduleMgr {
public:
    ScheduleMgr(const ScheduleMgr&) = delete;
    void operator=(const ScheduleMgr&) = delete;

    static ScheduleMgr& getInstance();

    void init();
    void shutdown();

    int addTask(const ScheduleTask& task);
    void onTimerExpired();
    int timerFd() const;

    static time_t parseTriggerAt(const std::string& scheduleDate, const std::string& scheduleTime);
    static std::vector<std::string> parsePaths(const char* pathsJson);
    static std::string encodePaths(const std::vector<std::string>& paths);

private:
    ScheduleMgr();
    ~ScheduleMgr();

    void reloadFromDb();
    void pushHeap(const ScheduleTask& task);
    void updateTimerfd();
    void executeTask(const ScheduleTask& task);

    std::priority_queue<ScheduleTask, std::vector<ScheduleTask>, ScheduleTaskCompare> m_heap;
    std::unordered_map<int, ScheduleTask> m_taskMap;
    int m_timerFd;
    int m_nextId;
};

#endif
