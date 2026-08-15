#ifndef CONTENTMGR_H
#define CONTENTMGR_H

#include <string>
#include <vector>
#include "json_bags.h"
#include "json_bags_ext.h"

class ContentMgr {
public:
    ContentMgr(const ContentMgr&) = delete;
    void operator=(const ContentMgr&) = delete;

    static ContentMgr& getInstance() {
        static ContentMgr instance;
        return instance;
    }

    bool connectToQt();
    int  qtFd() const { return m_qtFd; }
    void onQtDisconnected();

    void handlePushResources(const PushResourcesToDownloadBag& bag);
    void handlePushSchedule(const PushSchedulePlaylistBag& bag);
    void handleScreenshotRequest(int requestClientFd);
    void handleOtaUpdate(const OtaUpdateBag& bag);

    bool handleQtMessage(const char* line);

    bool writePlaylistJson(const std::vector<std::string>& localPaths);
    bool writeScheduleJson(const PushSchedulePlaylistBag& bag, const std::vector<std::string>& localPaths);
    void notifyDownloadReady(const char* cmd);

private:
    ContentMgr() = default;

    bool notifyQt(const char* jsonLine);
    void startDownload(const std::vector<std::string>& urls, bool notifyContentReady);
    bool uploadScreenshot(const std::string& localPath, std::string& serverPath);
    void sendScreenshotData(int requestClientFd, const std::string& serverPath);
    void sendOtaAck(const EmbeddedOtaUpdateAckBag& ack);
    std::string urlToLocalPath(const std::string& url) const;

    int m_qtFd = -1;
};

#endif
