#ifndef FDBUFFER_H
#define FDBUFFER_H

#include <cstring>

struct FdBuffer {
    char data[8192];
    int  len;

    FdBuffer() : len(0) {}

    int append(const char* src, int srcLen) {
        if (len + srcLen > (int)sizeof(data)) {
            len = 0;
            return -1;
        }
        memcpy(data + len, src, srcLen);
        len += srcLen;
        data[len] = 0;
        return len;
    }

    void consume(int consumed) {
        if (consumed >= len) {
            len = 0;
        } else {
            memmove(data, data + consumed, len - consumed);
            len -= consumed;
        }
    }
};

#endif
