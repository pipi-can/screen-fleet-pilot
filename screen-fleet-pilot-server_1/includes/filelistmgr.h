#ifndef FILELISTMGR_H
#define FILELISTMGR_H

#include <vector>
#include "json_bags.h"

class FileListMgr {
public:
    FileListMgr(const FileListMgr&) = delete;
    void operator=(const FileListMgr&) = delete;

    static FileListMgr& getInstance() {
        static FileListMgr instance;
        return instance;
    }

    void loadUploadFileList(std::vector<FileListEntry>& files);

private:
    FileListMgr() = default;
    ~FileListMgr() = default;
};

#endif
