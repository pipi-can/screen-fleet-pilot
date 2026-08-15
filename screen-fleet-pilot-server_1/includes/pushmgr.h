#ifndef PUSHMGR_H
#define PUSHMGR_H

#include <string>
#include <vector>
#include "json_bags_ext.h"

class PushMgr {
public:
    PushMgr(const PushMgr&) = delete;
    void operator=(const PushMgr&) = delete;

    static PushMgr& getInstance() {
        static PushMgr instance;
        return instance;
    }

    static std::string toHttpUrl(const std::string& relativePath);
    static std::vector<std::string> toHttpUrls(const std::vector<std::string>& relativePaths);

    bool pushResourcesToEmbedded(const std::string& uid,
                                 const std::vector<std::string>& relativePaths);
    bool pushSchedulePlaylist(const std::string& uid, const PushSchedulePlaylistBag& bag);

private:
    PushMgr() = default;
};

#endif
