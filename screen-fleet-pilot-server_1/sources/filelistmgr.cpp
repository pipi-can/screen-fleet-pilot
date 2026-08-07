#include "../includes/filelistmgr.h"
#include "global_def.h"
#include "logmgr.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string>

static LogMgr* logger = &LogMgr::getInstance();

void FileListMgr::loadUploadFileList(std::vector<FileListEntry>& files) {
    files.clear();

    DIR* dir = opendir(UPLOAD_DIR);
    if (!dir) {
        logger->logMsg(ERROR, std::string("failed to open upload dir: ") + UPLOAD_DIR, true);
        return;
    }

    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') {
            continue;
        }

        const std::string fullPath = std::string(UPLOAD_DIR) + "/" + ent->d_name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) {
            continue;
        }
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        FileListEntry entry;
        entry.path = std::string(UPLOAD_URL_PREFIX) + "/" + ent->d_name;
        entry.name = ent->d_name;
        entry.size = static_cast<int64_t>(st.st_size);
        files.push_back(std::move(entry));
    }

    closedir(dir);
}
