#include "../includes/client.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <bits/time.h>
#include <sys/statvfs.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

static int       g_qtFd = -1;
static Client*   g_client = NULL;
const char* DEVICE_VERSION = "1.0.5";

static char* url_encode_spaces(const char* url) {
    int extra = 0;
    for (const char* p = url; *p; p++) {
        if (*p == ' ') extra += 2;
    }
    char* out = malloc(strlen(url) + extra + 1);
    if (!out) return NULL;

    char* w = out;
    for (const char* p = url; *p; p++) {
        if (*p == ' ') {
            *w++ = '%'; *w++ = '2'; *w++ = '0';
        } else {
            *w++ = *p;
        }
    }
    *w = '\0';
    return out;
}

static int write_playlist_json(char** contents, int count) {
    if (!contents || count <= 0) {
        return -1;
    }

    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "cmd", json_object_new_string("content_ready"));

    struct json_object* arr = json_object_new_array();
    for (int i = 0; i < count; i++) {
        if (contents[i] && contents[i][0]) {
            json_object_array_add(arr, json_object_new_string(contents[i]));
        }
    }
    json_object_object_add(root, "paths", arr);

    const char* json = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);

    char buf[4096];
    int len = snprintf(buf, sizeof(buf), "%s\n", json);
    json_object_put(root);
    if (len < 0 || len >= (int)sizeof(buf)) {
        fprintf(stderr, "[client] playlist json too large\n");
        return -1;
    }

    int fd = open(PLAYLIST_JSON_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[client] write playlist failed");
        return -1;
    }

    int sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buf + sent, (size_t)(len - sent));
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("[client] write playlist failed");
            close(fd);
            return -1;
        }
        sent += (int)n;
    }
    close(fd);

    printf("[client] wrote %s\n", PLAYLIST_JSON_PATH);
    return 0;
}

static int url_to_local_path(const char* url, char* outPath, size_t outCap) {
    if (!url || !outPath || outCap == 0) {
        return -1;
    }
    const char* name = strrchr(url, '/');
    name = (name && name[1]) ? name + 1 : "download.bin";
    if (snprintf(outPath, outCap, CONTENT_DIR "/%s", name) >= (int)outCap) {
        return -1;
    }
    return 0;
}

static int write_schedule_json(struct json_object* params) {
    if (!params) {
        return -1;
    }

    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "cmd", json_object_new_string("schedule_playlist"));

    struct json_object* dateObj = NULL;
    struct json_object* timeObj = NULL;
    struct json_object* durObj = NULL;
    struct json_object* pathsObj = NULL;
    json_object_object_get_ex(params, "schedule_date", &dateObj);
    json_object_object_get_ex(params, "schedule_time", &timeObj);
    json_object_object_get_ex(params, "duration_sec", &durObj);
    json_object_object_get_ex(params, "paths", &pathsObj);

    if (dateObj) {
        json_object_object_add(root, "schedule_date",
                               json_object_new_string(json_object_get_string(dateObj)));
    }
    if (timeObj) {
        json_object_object_add(root, "schedule_time",
                               json_object_new_string(json_object_get_string(timeObj)));
    }
    if (durObj) {
        json_object_object_add(root, "duration_sec", json_object_new_int(json_object_get_int(durObj)));
    }

    struct json_object* localArr = json_object_new_array();
    if (pathsObj && json_object_get_type(pathsObj) == json_type_array) {
        struct json_object* urlArr = json_object_new_array();
        int len = json_object_array_length(pathsObj);
        for (int i = 0; i < len; i++) {
            struct json_object* item = json_object_array_get_idx(pathsObj, i);
            if (!item || json_object_get_type(item) != json_type_string) {
                continue;
            }
            const char* url = json_object_get_string(item);
            if (!url || !url[0]) {
                continue;
            }
            json_object_array_add(urlArr, json_object_new_string(url));

            char localPath[256];
            if (url_to_local_path(url, localPath, sizeof(localPath)) == 0) {
                json_object_array_add(localArr, json_object_new_string(localPath));
            }
        }
        json_object_object_add(root, "paths", urlArr);
    }
    json_object_object_add(root, "local_paths", localArr);

    const char* json = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char buf[8192];
    int len = snprintf(buf, sizeof(buf), "%s\n", json);
    json_object_put(root);
    if (len < 0 || len >= (int)sizeof(buf)) {
        fprintf(stderr, "[client] schedule json too large\n");
        return -1;
    }

    system("mkdir -p " CONTENT_DIR);

    int fd = open(SCHEDULE_JSON_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[client] write schedule failed");
        return -1;
    }

    int sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buf + sent, (size_t)(len - sent));
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("[client] write schedule failed");
            close(fd);
            return -1;
        }
        sent += (int)n;
    }
    close(fd);

    printf("[client] wrote %s\n", SCHEDULE_JSON_PATH);
    return 0;
}

static void client_start_download_thread(char** urlList, int count, int notify_qt) {
    if (!urlList || count <= 0) {
        free(urlList);
        return;
    }

    download_context* ctx = malloc(sizeof(download_context));
    if (!ctx) {
        for (int i = 0; i < count; i++) {
            free(urlList[i]);
        }
        free(urlList);
        return;
    }
    ctx->urls = urlList;
    ctx->count = count;
    ctx->notify_qt = notify_qt;

    pthread_t tid;
    if (pthread_create(&tid, NULL, batch_download_thread, ctx) == 0) {
        pthread_detach(tid);
    } else {
        for (int i = 0; i < count; i++) {
            free(urlList[i]);
        }
        free(urlList);
        free(ctx);
    }
}

static char** collect_url_list(struct json_object* pathsObj, int* outCount) {
    if (!pathsObj || json_object_get_type(pathsObj) != json_type_array || !outCount) {
        return NULL;
    }

    int len = json_object_array_length(pathsObj);
    if (len <= 0) {
        return NULL;
    }

    char** urlList = malloc(sizeof(char*) * len);
    if (!urlList) {
        return NULL;
    }

    int n = 0;
    for (int i = 0; i < len; i++) {
        struct json_object* item = json_object_array_get_idx(pathsObj, i);
        if (!item || json_object_get_type(item) != json_type_string) {
            continue;
        }
        const char* path = json_object_get_string(item);
        if (!path || !path[0]) {
            continue;
        }
        urlList[n] = strdup(path);
        if (!urlList[n]) {
            break;
        }
        n++;
    }

    if (n == 0) {
        free(urlList);
        return NULL;
    }

    *outCount = n;
    return urlList;
}

typedef struct ota_context {
    Client* client;
    char*   url;
    char    server_path[256];
    char    expected_md5[33];
    char    device_uid[128];
} ota_context;

static int is_hex_md5(const char* s) {
    if (!s || strlen(s) != 32) {
        return 0;
    }
    for (int i = 0; s[i]; i++) {
        if (!isxdigit((unsigned char)s[i])) {
            return 0;
        }
    }
    return 1;
}

static int file_md5_hex(const char* path, char* out, size_t outCap) {
    if (!path || !out || outCap < 33) {
        return -1;
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "md5sum '%s'", path);
    FILE* fp = popen(cmd, "r");
    if (!fp) {
        return -1;
    }

    char line[128];
    if (!fgets(line, sizeof(line), fp)) {
        pclose(fp);
        return -1;
    }
    pclose(fp);

    if (strlen(line) < 32) {
        return -1;
    }
    strncpy(out, line, 32);
    out[32] = '\0';
    return 0;
}

static void client_send_ota_update_ack(Client* c, int code, int result,
                                       const char* serverPath,
                                       const char* localPath, const char* msg,
                                       const char* deviceUid) {
    if (!c) {
        return;
    }

    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "source", json_object_new_string("embedded"));
    json_object_object_add(root, "cmd", json_object_new_string("ota_update_ack"));
    json_object_object_add(root, "seq", json_object_new_int(++c->msgSeq));

    struct json_object* p = json_object_new_object();
    json_object_object_add(p, "code", json_object_new_int(code));
    json_object_object_add(p, "result", json_object_new_int(result));
    json_object_object_add(p, "msg", json_object_new_string(msg ? msg : ""));
    if (serverPath && serverPath[0]) {
        json_object_object_add(p, "path", json_object_new_string(serverPath));
    }
    if (localPath && localPath[0]) {
        json_object_object_add(p, "local_path", json_object_new_string(localPath));
    }
    json_object_object_add(p, "device_uid",
                           json_object_new_string(deviceUid ? deviceUid : ""));
    json_object_object_add(root, "params", p);

    client_send_json(c, root);
    json_object_put(root);
    printf("[client] ota_update_ack code=%d result=%d path=%s device_uid=%s\n",
           code, result, serverPath ? serverPath : "", deviceUid ? deviceUid : "");
}

static void* ota_download_thread(void* arg) {
    ota_context* ctx = (ota_context*)arg;
    if (!ctx || !ctx->client || !ctx->url) {
        free(ctx);
        return NULL;
    }

    Client* c = ctx->client;
    const char* url = ctx->url;
    char localPath[256] = {0};
    int code = -1;
    int result = 0;
    const char* msg = "download failed";

    system("mkdir -p " FIRMWARE_DIR);

    char* fetchUrl = url_encode_spaces(url);
    if (!fetchUrl) {
        client_send_ota_update_ack(c, code, result, ctx->server_path, "",
                                   "url encode failed", ctx->device_uid);
        free(ctx->url);
        free(ctx);
        return NULL;
    }

    const char* name = strrchr(url, '/');
    name = (name && name[1]) ? name + 1 : "firmware.bin";
    snprintf(localPath, sizeof(localPath), FIRMWARE_DIR "/%s", name);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -fsSL -o '%s' '%s'", localPath, fetchUrl);

    int ret = system(cmd);
    printf("[client] ota download %s -> %s ret=%d\n", fetchUrl, localPath, ret);
    free(fetchUrl);

    if (ret == 0) {
        char local_md5[33] = {0};
        if (file_md5_hex(localPath, local_md5, sizeof(local_md5)) != 0) {
            code = -1;
            msg = "md5 calc failed";
        } else if (strcasecmp(local_md5, ctx->expected_md5) != 0) {
            code = -1;
            msg = "md5 mismatch";
            unlink(localPath);
            printf("[client] ota md5 mismatch expect=%s actual=%s\n",
                   ctx->expected_md5, local_md5);
        } else {
            code = 0;
            result = 1;
            msg = "ok";
            printf("[client] ota md5 ok: %s\n", local_md5);
        }
    }

    client_send_ota_update_ack(c, code, result, ctx->server_path, localPath, msg,
                               ctx->device_uid);

    if (result == 1) {
        char scriptCmd[512];
        snprintf(scriptCmd, sizeof(scriptCmd),
                 "python3 " OTA_SCRIPT_PATH " '%s'", localPath);
        int sret = system(scriptCmd);
        printf("[client] ota_update script ret=%d\n", sret);
    }

    free(ctx->url);
    free(ctx);
    return NULL;
}

static int notify_qt(struct json_object* root);
static void client_on_qt_recv(Client* c);
static int client_connect_qt(Client* c);
static void client_handle_qt_reply(Client* c, const char* jsonStr);

static int notify_qt(struct json_object* root) {
    if (g_qtFd < 0) {
        fprintf(stderr, "[client] notify qt: not connected\n");
        return -1;
    }

    const char* json = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char packet[4096];
    int total = snprintf(packet, sizeof(packet), "%s\n", json);
    if (total < 0 || total >= (int)sizeof(packet)) {
        fprintf(stderr, "[client] notify qt: message too large\n");
        return -1;
    }

    int sent = 0;
    while (sent < total) {
        ssize_t n = write(g_qtFd, packet + sent, (size_t)(total - sent));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            perror("[client] notify qt: write failed");
            return -1;
        }
        sent += (int)n;
    }
    return 0;
}

static int notify_schedule_ready(void) {
    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "cmd", json_object_new_string("schedule_ready"));

    int ret = notify_qt(root);
    json_object_put(root);
    if (ret == 0) {
        printf("[client] notify qt: schedule_ready\n");
    }
    return ret;
}

static void client_send_screenshot_data(Client* c, int requestClientFd, const char* path) {
    if (!c || requestClientFd < 0) {
        fprintf(stderr, "[client] screenshot_data: invalid request_client_fd\n");
        return;
    }

    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "source", json_object_new_string("embedded"));
    json_object_object_add(root, "cmd", json_object_new_string("screenshot_data"));
    json_object_object_add(root, "seq", json_object_new_int(++c->msgSeq));

    struct json_object* p = json_object_new_object();
    json_object_object_add(p, "request_client_fd", json_object_new_int(requestClientFd));
    if (path && path[0]) {
        json_object_object_add(p, "path", json_object_new_string(path));
    }
    json_object_object_add(root, "params", p);

    client_send_json(c, root);
    json_object_put(root);
    printf("[client] screenshot_data sent, request_client_fd=%d path=%s\n",
           requestClientFd, path ? path : "");
}

static int upload_screenshot_to_server(Client* c, const char* localPath,
                                       char* serverPath, size_t serverPathCap) {
    if (!c || !localPath || !localPath[0] || !c->serverIp[0]) {
        return -1;
    }

    char remoteName[192];
    snprintf(remoteName, sizeof(remoteName), "%s_%ld.png",
             c->deviceUid, (long)time(NULL));

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "curl -fsSL -T '%s' 'http://%s%s/%s'",
             localPath, c->serverIp, SCREENSHOT_URL_PREFIX, remoteName);

    int ret = system(cmd);
    printf("[client] screenshot upload ret=%d cmd=%s\n", ret, cmd);
    if (ret != 0) {
        return -1;
    }

    snprintf(serverPath, serverPathCap, "%s/%s", SCREENSHOT_URL_PREFIX, remoteName);
    return 0;
}

static void client_handle_qt_reply(Client* c, const char* jsonStr) {
    struct json_object* root = json_tokener_parse(jsonStr);
    if (!root) {
        return;
    }

    struct json_object* cmdObj = NULL;
    if (!json_object_object_get_ex(root, "cmd", &cmdObj)) {
        json_object_put(root);
        return;
    }
    const char* cmd = json_object_get_string(cmdObj);

    if (strcmp(cmd, "screenshot_ready") == 0) {
        struct json_object* idObj = NULL;
        struct json_object* pathObj = NULL;
        json_object_object_get_ex(root, "device_id", &idObj);
        json_object_object_get_ex(root, "path", &pathObj);

        int clientId = idObj ? json_object_get_int(idObj) : -1;
        const char* path = pathObj ? json_object_get_string(pathObj) : NULL;

        char serverPath[256];
        if (!path || upload_screenshot_to_server(c, path, serverPath, sizeof(serverPath)) < 0) {
            fprintf(stderr, "[client] screenshot upload failed: %s\n", path ? path : "");
            json_object_put(root);
            return;
        }
        client_send_screenshot_data(c, clientId, serverPath);
    }

    json_object_put(root);
}

int notify_content(char** contents, int count) {
    if (!contents || count == 0) {
        return -1;
    }
    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "cmd", json_object_new_string("content_ready"));
    struct json_object* arr = json_object_new_array();
    for (int i = 0; i < count; i++) {
        if (contents[i] && contents[i][0]) {
            json_object_array_add(arr, json_object_new_string(contents[i]));
        }
    }
    json_object_object_add(root, "paths", arr);

    int ret = notify_qt(root);
    json_object_put(root);
    if (ret == 0) {
        printf("[client] notify qt: content_ready\n");
    }
    return ret;
}

void* batch_download_thread(void* arg) {
    download_context* ctx = (download_context*)arg;
    if (!ctx) return NULL;

    char** urls = ctx->urls;
    int count = ctx->count;

    char** local_paths = (char**)malloc(sizeof(char*) * ctx->count);
    int ok_count = 0;
    memset(local_paths, 0, sizeof(char*) * ctx->count);

    system("mkdir -p " CONTENT_DIR);

    for (int i = 0; i < count; i++) {
        if (!urls[i]) continue;

        const char* url = urls[i];
        char* fetchUrl = url_encode_spaces(url);
        if (!fetchUrl) continue;

        const char* name = strrchr(url, '/');
        name = (name && name[1]) ? name + 1 : "download.bin";

        char outPath[256];
        snprintf(outPath, sizeof(outPath), CONTENT_DIR "/%s", name);

        if (!ctx->notify_qt && access(outPath, F_OK) == 0) {
            printf("[client] schedule skip existing: %s\n", outPath);
            continue;
        }

        char cmd[768];
        snprintf(cmd, sizeof(cmd),
                 "curl -fsSL -o '%s' '%s'", outPath, fetchUrl);

        int ret = system(cmd);
        printf("[client] download [%d] %s -> %s ret=%d\n", i, fetchUrl, name, ret);
        if (ret == 0) {
            local_paths[ok_count++] = strdup(outPath);
        }
        free(fetchUrl);
    }

    if (ctx->notify_qt && ok_count > 0) {
        if (write_playlist_json(local_paths, ok_count) < 0) {
            perror("[client] write playlist json failed");
        }
        if (notify_content(local_paths, ok_count) < 0) {
            perror("[client]: notify content failed");
        }
    } else if (!ctx->notify_qt) {
        printf("[client] schedule download finished, %d new file(s)\n", ok_count);
        if (notify_schedule_ready() < 0) {
            fprintf(stderr, "[client] notify qt schedule_ready failed\n");
        }
    }

    for (int i = 0; i < ok_count; i++) {
        free(local_paths[i]);
    }
    for (int i = 0; i < count; i++) {
        free(urls[i]);
    }
    free(urls);
    free(local_paths);
    free(ctx);
    return NULL;
}

// ── 工具 ──
static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// ── FdBuffer ──
static void fdbuf_append(FdBuffer* buf, const char* src, int srcLen) {
    if (buf->len + srcLen > (int)sizeof(buf->data)) {
        buf->len = 0;
        return;
    }
    memcpy(buf->data + buf->len, src, srcLen);
    buf->len += srcLen;
    buf->data[buf->len] = '\0';
}

static void fdbuf_consume(FdBuffer* buf, int consumed) {
    if (consumed >= buf->len) {
        buf->len = 0;
    } else {
        memmove(buf->data, buf->data + consumed, buf->len - consumed);
        buf->len -= consumed;
    }
}

// ── 初始化 + 连接 + 发注册包 ──
static int client_connect_qt(Client* c) {
    while (c->running) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("[client] qt socket");
            sleep(RECONNECT_INTERVAL_SEC);
            continue;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("[client] qt connect");
            close(fd);
            printf("[client] connect qt failed, retry in %ds...\n", RECONNECT_INTERVAL_SEC);
            sleep(RECONNECT_INTERVAL_SEC);
            continue;
        }

        set_nonblocking(fd);
        g_qtFd = fd;
        c->qtRecvBuf.len = 0;

        struct epoll_event ev = {
            .events = EPOLLIN,
            .data.fd = fd,
        };
        if (epoll_ctl(c->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
            perror("[client] qt epoll_ctl");
            close(fd);
            g_qtFd = -1;
            return -1;
        }

        printf("[client] connected to qt, fd=%d\n", fd);
        return 0;
    }
    return -1;
}

static void client_on_qt_recv(Client* c) {
    if (g_qtFd < 0) {
        return;
    }

    char buf[4096];
    int n = (int)read(g_qtFd, buf, sizeof(buf));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        perror("[client] qt recv");
        return;
    }
    if (n == 0) {
        printf("[client] qt disconnected\n");
        epoll_ctl(c->epfd, EPOLL_CTL_DEL, g_qtFd, NULL);
        close(g_qtFd);
        g_qtFd = -1;
        c->qtRecvBuf.len = 0;
        return;
    }

    fdbuf_append(&c->qtRecvBuf, buf, n);

    while (1) {
        char* nl = memchr(c->qtRecvBuf.data, '\n', (size_t)c->qtRecvBuf.len);
        if (!nl) {
            break;
        }

        int frameLen = (int)(nl - c->qtRecvBuf.data);
        if (frameLen > 0 && c->qtRecvBuf.data[frameLen - 1] == '\r') {
            frameLen--;
        }

        if (frameLen > 0) {
            c->qtRecvBuf.data[frameLen] = '\0';
            printf("[client] qt recv: %s\n", c->qtRecvBuf.data);
            client_handle_qt_reply(c, c->qtRecvBuf.data);
        }
        fdbuf_consume(&c->qtRecvBuf, frameLen + 1);
    }
}

static int client_connect_server(Client* c, const char* ip, int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    while (c->running) {
        c->serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (c->serverFd < 0) {
            perror("socket");
            sleep(RECONNECT_INTERVAL_SEC);
            continue;
        }

        if (connect(c->serverFd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            return 0;
        }

        perror("connect");
        printf("[client] connect %s:%d failed, retry in %ds...\n",
               ip, port, RECONNECT_INTERVAL_SEC);
        close(c->serverFd);
        c->serverFd = -1;
        sleep(RECONNECT_INTERVAL_SEC);
    }
    return -1;
}

int client_init(Client* c, const char* ip, int port,
                const char* name, const char* group, const char* uid) {
    (void)uid;
    g_client = c;
    g_qtFd = -1;
    memset(c, 0, sizeof(Client));
    c->running    = 1;
    c->serverFd   = -1;
    c->deviceId   = -1;
    c->registered = 0;

    strncpy(c->deviceName,    name  ? name  : "unnamed", 63);
    strncpy(c->deviceGroup,   group ? group : "default", 63);
    strncpy(c->deviceVersion, "1.0", 31);
    strncpy(c->serverIp, ip ? ip : "", sizeof(c->serverIp) - 1);
    get_mac_address(c->deviceUid);
    if (!c->deviceUid[0]) {
        snprintf(c->deviceUid, sizeof(c->deviceUid),
                 "%s@%d", c->deviceName, getpid());
    }
    printf("[client] device_uid: %s\n", c->deviceUid);

    // epoll
    c->epfd = epoll_create1(0);
    if (c->epfd < 0) { perror("epoll_create1"); return -1; }

    // heartbeatTimer
    c->heartbeatTimerFd = timerfd_create(1, TFD_NONBLOCK | TFD_CLOEXEC);
    if (c->heartbeatTimerFd < 0) { perror("timerfd_create"); return -1; }

    
    struct epoll_event timerEv = {
        .events = EPOLLIN,
        .data.fd = c->heartbeatTimerFd,
    };

    epoll_ctl(c->epfd, EPOLL_CTL_ADD, c->heartbeatTimerFd, &timerEv);

    if (client_connect_qt(c) < 0) {
        close(c->epfd);
        return -1;
    }

    if (client_connect_server(c, ip, port) < 0) {
        close(g_qtFd);
        g_qtFd = -1;
        close(c->epfd);
        return -1;
    }

    set_nonblocking(c->serverFd);

    struct epoll_event ev;
    ev.data.fd = c->serverFd;
    ev.events  = EPOLLIN;
    epoll_ctl(c->epfd, EPOLL_CTL_ADD, c->serverFd, &ev);

    printf("[client] connected to %s:%d, fd=%d, uid=%s\n", ip, port, c->serverFd, c->deviceUid);

    // 连接后立刻发送注册包
    client_send_register(c);

    return 0;
}

// ── 发送 JSON（加 \n 分隔符，匹配服务端协议） ──
void client_send_json(Client* c, struct json_object* obj) {
    if (c->serverFd < 0) return;

    const char* str = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);

    char packet[4096];
    int  total = snprintf(packet, sizeof(packet), "%s\n", str);
    int  sent  = 0;
    const char* ptr = packet;

    while (sent < total) {
        int n = send(c->serverFd, ptr, total - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("send");
            return;
        }
        sent += n;
        ptr  += n;
    }
    printf("[client] sent: %s\n", str);
}

// ── 发送注册包 ──
void client_send_register(Client* c) {
    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "source", json_object_new_string("embedded"));
    json_object_object_add(root, "cmd",    json_object_new_string("register"));
    json_object_object_add(root, "seq",    json_object_new_int(0));

    struct json_object* params = json_object_new_object();
    json_object_object_add(params, "name",       json_object_new_string(c->deviceName));
    json_object_object_add(params, "group",      json_object_new_string(c->deviceGroup));
    json_object_object_add(params, "version",    json_object_new_string(c->deviceVersion));
    json_object_object_add(params, "device_uid", json_object_new_string(c->deviceUid));
    json_object_object_add(root, "params", params);

    client_send_json(c, root);
    json_object_put(root);
}

void client_send_heartbeat(Client* c) {
    if (!c->registered || c->serverFd < 0)
        return;

    char cpuRaw[256] = {0};
    get_cpu_temp(cpuRaw);
    char cpuTempStr[16] = "-1";
    if (cpuRaw[0]) {
        snprintf(cpuTempStr, sizeof(cpuTempStr), "%d", atoi(cpuRaw) / 1000);
    }

    int memUsage = -1, diskFreeMb = -1;
    get_mem_usage(&memUsage);
    get_disk_free_mb(&diskFreeMb);

    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "source",    json_object_new_string("embedded"));
    json_object_object_add(root, "cmd",       json_object_new_string("heartbeat"));
    json_object_object_add(root, "seq",       json_object_new_int(++c->heartbeatSeq));
    json_object_object_add(root, "device_id", json_object_new_int(c->deviceId));

    struct json_object* params = json_object_new_object();
    json_object_object_add(params, "cpu_temp",        json_object_new_string(cpuTempStr));
    json_object_object_add(params, "mem_usage",       json_object_new_int(memUsage));
    json_object_object_add(params, "disk_free_mb",    json_object_new_int(diskFreeMb));
    json_object_object_add(params, "current_content", json_object_new_string(c->deviceName));
    json_object_object_add(params, "timestamp", json_object_new_int((int)time(NULL)));
    
    json_object_object_add(root, "params", params);
    client_send_json(c, root);
    json_object_put(root);

    printf("[client] heartbeat #%d cpu:%s mem:%d%%\n",
           c->heartbeatSeq, cpuTempStr, memUsage);
}

static void client_stop_heartbeat(Client* c) {
    if (!c || !c->registered) {
        return;
    }

    c->registered = 0;

    struct itimerspec its = {0};
    timerfd_settime(c->heartbeatTimerFd, 0, &its, NULL);

    uint64_t exp;
    while (read(c->heartbeatTimerFd, &exp, sizeof(exp)) > 0) {
    }

    printf("[client] heartbeat stopped\n");
}

// ── 接收数据（按 \n 切帧，跟服务端同样的分包逻辑） ──
void client_on_recv(Client* c) {
    char buf[4096];
    int n = recv(c->serverFd, buf, sizeof(buf), 0);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        perror("recv");
        client_stop_heartbeat(c);
        c->running = 0;
        return;
    }
    if (n == 0) {
        printf("[client] server closed connection\n");
        client_stop_heartbeat(c);
        c->running = 0;
        return;
    }

    fdbuf_append(&c->recvBuf, buf, n);

    // 按 \n 切帧
    while (1) {
        char* nl = memchr(c->recvBuf.data, '\n', c->recvBuf.len);
        if (!nl) break;

        int frameLen = (int)(nl - c->recvBuf.data);
        if (frameLen > 0 && c->recvBuf.data[frameLen - 1] == '\r') {
            frameLen--;
        }

        if (frameLen > 0) {
            c->recvBuf.data[frameLen] = '\0';
            printf("[client] recv: %s\n", c->recvBuf.data);
            client_handle_message(c, c->recvBuf.data);
        }
        fdbuf_consume(&c->recvBuf, frameLen + 1);
    }
}

// ── 消息分发 ──
void client_handle_message(Client* c, const char* jsonStr) {
    struct json_object* root = json_tokener_parse(jsonStr);
    if (!root) return;

    struct json_object* cmdObj = NULL;
    if (!json_object_object_get_ex(root, "cmd", &cmdObj)) {
        json_object_put(root);
        return;
    }
    const char* cmd = json_object_get_string(cmdObj);

    struct json_object* params = NULL;
    json_object_object_get_ex(root, "params", &params);

    if (strcmp(cmd, "register_ack") == 0) {
        client_handle_register_ack(c, params);
    } else if (strcmp(cmd, "update_embedded_info") == 0) {
        client_handle_update_embedded_info(c, params);
    } else if (strcmp(cmd, "push_resources_to_download") == 0) {
        client_handle_push_resources_to_download(c, params);
    } else if (strcmp(cmd, "push_schedule_playlist") == 0) {
        client_handle_push_schedule_playlist(c, params);
    } else if (strcmp(cmd, "request_screenshot") == 0) {
        client_handle_request_screenshot(c, params);
    } else if (strcmp(cmd, "ota_update") == 0) {
        client_handle_ota_update(c, params);
    }

    json_object_put(root);
}

// ── 处理注册回包 ──
void client_handle_register_ack(Client* c, struct json_object* params) {
    if (!params) return;

    struct json_object* codeObj = NULL;
    json_object_object_get_ex(params, "code", &codeObj);
    int code = codeObj ? json_object_get_int(codeObj) : -1;

    struct json_object* idObj = NULL;
    json_object_object_get_ex(params, "device_id", &idObj);
    int id = idObj ? json_object_get_int(idObj) : -1;

    if (code == 0) {
        c->deviceId   = id;
        c->registered = 1;
        printf("[client] register ok, device_id=%d\n", id);
        struct itimerspec its = {
            .it_value    = { HEARTBEAT_INTERVAL_SEC, 0 }, 
            .it_interval = { HEARTBEAT_INTERVAL_SEC, 0 },
        };
        timerfd_settime(c->heartbeatTimerFd, 0, &its, NULL); // start timer
        watchdog_start(c);
    } else {
        printf("[client] register failed, code=%d\n", code);
    }
}

void client_handle_update_embedded_info(Client* c, struct json_object* params) {
    if (!params) return;

    struct json_object* nameObj = NULL, *groupObj = NULL, *senderObj = NULL;
    json_object_object_get_ex(params, "name", &nameObj);
    json_object_object_get_ex(params, "group", &groupObj);
    json_object_object_get_ex(params, "sender", &senderObj);

    if (nameObj) {
        strncpy(c->deviceName, json_object_get_string(nameObj), sizeof(c->deviceName) - 1);
        c->deviceName[sizeof(c->deviceName) - 1] = '\0';
    }
    if (groupObj) {
        strncpy(c->deviceGroup, json_object_get_string(groupObj), sizeof(c->deviceGroup) - 1);
        c->deviceGroup[sizeof(c->deviceGroup) - 1] = '\0';
    }
    int senderFd = senderObj ? json_object_get_int(senderObj) : -1;

    printf("[client] update info: name=%s group=%s\n", c->deviceName, c->deviceGroup);

    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "source", json_object_new_string("embedded"));
    json_object_object_add(root, "cmd",    json_object_new_string("update_info_ack"));
    json_object_object_add(root, "seq",    json_object_new_int(++c->msgSeq));

    struct json_object* ackParams = json_object_new_object();
    json_object_object_add(ackParams, "sender", json_object_new_int(senderFd));
    json_object_object_add(ackParams, "group",  json_object_new_string(c->deviceGroup));
    json_object_object_add(ackParams, "name",   json_object_new_string(c->deviceName));
    json_object_object_add(ackParams, "msg",    json_object_new_string("ok"));
    json_object_object_add(root, "params", ackParams);

    client_send_json(c, root);
    json_object_put(root);
}

void client_handle_push_resources_to_download(Client* c, struct json_object* params) {
    (void)c;
    if (!params) {
        printf("[client] push_resources_to_download: missing params\n");
        return;
    }

    struct json_object* pathsObj = NULL;
    if (!json_object_object_get_ex(params, "paths", &pathsObj) ||
        json_object_get_type(pathsObj) != json_type_array) {
        printf("[client] push_resources_to_download: invalid paths\n");
        return;
    }

    int n = 0;
    char** urlList = collect_url_list(pathsObj, &n);
    if (!urlList) {
        return;
    }

    printf("[client] push_resources_to_download: %d path(s)\n", n);
    for (int i = 0; i < n; i++) {
        printf("[client]   [%d] %s\n", i, urlList[i]);
    }

    client_start_download_thread(urlList, n, 1);
}

void client_handle_push_schedule_playlist(Client* c, struct json_object* params) {
    (void)c;
    if (!params) {
        printf("[client] push_schedule_playlist: missing params\n");
        return;
    }

    struct json_object* pathsObj = NULL;
    struct json_object* dateObj = NULL;
    struct json_object* timeObj = NULL;
    struct json_object* durObj = NULL;
    json_object_object_get_ex(params, "paths", &pathsObj);
    json_object_object_get_ex(params, "schedule_date", &dateObj);
    json_object_object_get_ex(params, "schedule_time", &timeObj);
    json_object_object_get_ex(params, "duration_sec", &durObj);

    if (!pathsObj || json_object_get_type(pathsObj) != json_type_array) {
        printf("[client] push_schedule_playlist: invalid paths\n");
        return;
    }

    const char* scheduleDate = dateObj ? json_object_get_string(dateObj) : "";
    const char* scheduleTime = timeObj ? json_object_get_string(timeObj) : "";
    int durationSec = durObj ? json_object_get_int(durObj) : 0;

    printf("[client] push_schedule_playlist: date=%s time=%s duration=%d\n",
           scheduleDate, scheduleTime, durationSec);

    if (write_schedule_json(params) < 0) {
        fprintf(stderr, "[client] push_schedule_playlist: save task failed\n");
        return;
    }

    int n = 0;
    char** urlList = collect_url_list(pathsObj, &n);
    if (!urlList) {
        return;
    }

    printf("[client] push_schedule_playlist: %d url(s), download missing only\n", n);
    for (int i = 0; i < n; i++) {
        char localPath[256];
        if (url_to_local_path(urlList[i], localPath, sizeof(localPath)) == 0 &&
            access(localPath, F_OK) == 0) {
            printf("[client]   [%d] cached %s\n", i, localPath);
        } else {
            printf("[client]   [%d] need %s\n", i, urlList[i]);
        }
    }

    client_start_download_thread(urlList, n, 0);
}

void client_handle_ota_update(Client* c, struct json_object* params) {
    if (!c || !params) {
        printf("[client] ota_update: missing params\n");
        return;
    }

    const char* deviceUid = "";
    struct json_object* uidObj = NULL;
    json_object_object_get_ex(params, "device_uid", &uidObj);
    if (uidObj && json_object_get_type(uidObj) == json_type_string) {
        deviceUid = json_object_get_string(uidObj);
    }

    struct json_object* pathObj = NULL;
    json_object_object_get_ex(params, "path", &pathObj);
    if (!pathObj || json_object_get_type(pathObj) != json_type_string) {
        printf("[client] ota_update: invalid path\n");
        client_send_ota_update_ack(c, -1, 0, "", "", "invalid path", deviceUid);
        return;
    }

    const char* path = json_object_get_string(pathObj);
    if (!path || !path[0]) {
        printf("[client] ota_update: empty path\n");
        client_send_ota_update_ack(c, -1, 0, "", "", "empty path", deviceUid);
        return;
    }

    if (!c->serverIp[0]) {
        fprintf(stderr, "[client] ota_update: no server ip\n");
        client_send_ota_update_ack(c, -1, 0, path, "", "no server ip", deviceUid);
        return;
    }

    struct json_object* md5Obj = NULL;
    json_object_object_get_ex(params, "md5", &md5Obj);
    if (!md5Obj || json_object_get_type(md5Obj) != json_type_string) {
        printf("[client] ota_update: missing md5\n");
        client_send_ota_update_ack(c, -1, 0, path, "", "missing md5", deviceUid);
        return;
    }
    const char* md5 = json_object_get_string(md5Obj);
    if (!is_hex_md5(md5)) {
        printf("[client] ota_update: invalid md5\n");
        client_send_ota_update_ack(c, -1, 0, path, "", "invalid md5", deviceUid);
        return;
    }

    char url[512];
    if (path[0] == '/') {
        snprintf(url, sizeof(url), "http://%s%s", c->serverIp, path);
    } else {
        snprintf(url, sizeof(url), "http://%s/%s", c->serverIp, path);
    }

    ota_context* ctx = malloc(sizeof(ota_context));
    if (!ctx) {
        client_send_ota_update_ack(c, -1, 0, path, "", "no memory", deviceUid);
        return;
    }

    ctx->client = c;
    ctx->url = strdup(url);
    ctx->device_uid[0] = '\0';
    if (deviceUid && deviceUid[0]) {
        strncpy(ctx->device_uid, deviceUid, sizeof(ctx->device_uid) - 1);
        ctx->device_uid[sizeof(ctx->device_uid) - 1] = '\0';
    }
    strncpy(ctx->server_path, path, sizeof(ctx->server_path) - 1);
    ctx->server_path[sizeof(ctx->server_path) - 1] = '\0';
    strncpy(ctx->expected_md5, md5, sizeof(ctx->expected_md5) - 1);
    ctx->expected_md5[sizeof(ctx->expected_md5) - 1] = '\0';

    if (!ctx->url) {
        free(ctx);
        client_send_ota_update_ack(c, -1, 0, path, "", "no memory", deviceUid);
        return;
    }

    printf("[client] ota_update start: %s md5=%s device_uid=%s\n",
           url, ctx->expected_md5, ctx->device_uid);

    pthread_t tid;
    if (pthread_create(&tid, NULL, ota_download_thread, ctx) == 0) {
        pthread_detach(tid);
    } else {
        free(ctx->url);
        free(ctx);
        client_send_ota_update_ack(c, -1, 0, path, "", "thread create failed", deviceUid);
    }
}

void client_handle_request_screenshot(Client* c, struct json_object* params) {
    int requestClientFd = -1;
    if (params) {
        struct json_object* fdObj = NULL;
        if (json_object_object_get_ex(params, "request_client_fd", &fdObj)) {
            requestClientFd = json_object_get_int(fdObj);
        } else {
            struct json_object* idObj = NULL;
            if (json_object_object_get_ex(params, "device_id", &idObj)) {
                requestClientFd = json_object_get_int(idObj);
            }
        }
    }

    struct json_object* root = json_object_new_object();
    json_object_object_add(root, "cmd", json_object_new_string("screenshot_request"));
    json_object_object_add(root, "device_id", json_object_new_int(requestClientFd));

    printf("[client] request_screenshot from server, request_client_fd=%d\n", requestClientFd);
    if (notify_qt(root) < 0) {
        fprintf(stderr, "[client] notify qt screenshot failed\n");
    }
    json_object_put(root);
}

// ── 主循环 ──
void client_run(Client* c) {
    struct epoll_event events[16];

    printf("[client] entering epoll loop, epfd=%d serverFd=%d qtFd=%d version=%s\n",
           c->epfd, c->serverFd, g_qtFd, DEVICE_VERSION);

    while (c->running) {
        int n = epoll_wait(c->epfd, events, 16, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == c->serverFd) {
                client_on_recv(c);
            } else if (events[i].data.fd == g_qtFd) {
                client_on_qt_recv(c);
            } else if (events[i].data.fd == c->heartbeatTimerFd) {
                if (!c->running || !c->registered) {
                    continue;
                }
                printf("[client]: heartbeat timer out\n");
                uint64_t exp;
                read(c->heartbeatTimerFd, &exp, sizeof(exp));
                client_send_heartbeat(c);
                watchdog_feed(c);
            }
        }
    }
}

void client_destroy(Client* c) {
    client_stop_heartbeat(c);
    if (c->serverFd >= 0) close(c->serverFd);
    if (g_qtFd >= 0) {
        close(g_qtFd);
        g_qtFd = -1;
    }
    if (c->epfd >= 0) close(c->epfd);
    g_client = NULL;
}

void get_cpu_temp(char* buf) {
    int fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY, 0664);
    if (fd < 0) {
        return ;
    }

    memset(buf, 0, 256);
    int ret = 0;
    if ((ret = read(fd, buf, 256)) < 0) {
        perror("read cpu temp failed");
        close(fd);
        return ;
    }

    buf[ret] = '\0';
    close(fd);
}

void get_mem_usage(int* buf) {
    int fd = open("/proc/meminfo", O_RDONLY, 0664);
    if (fd < 0) {
        return;
    }

    char data[1024];
    memset(data, 0, sizeof(data));
    int ret = read(fd, data, sizeof(data) - 1);
    if (ret < 0) {
        perror("read meminfo failed");
        close(fd);
        return;
    }
    data[ret] = '\0';
    close(fd);

    long total = 0, available = 0;
    char* line = data;
    while (line && *line) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %ld kB", &total);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line, "MemAvailable: %ld kB", &available);
        }
        char* nl = strchr(line, '\n');
        line = nl ? nl + 1 : NULL;
    }

    if (total == 0) {
        return;
    }
    *buf = (int)(100 - (available * 100 / total));
}

void get_disk_free_mb(int* buf) {
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) {
        return;
    }
    *buf = (int)((stat.f_bavail * stat.f_frsize) / (1024 * 1024));
}

static int read_mac_from_iface(const char* iface, char* buf, int bufsize) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", iface);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    char raw[32];
    memset(raw, 0, sizeof(raw));
    int ret = read(fd, raw, sizeof(raw) - 1);
    close(fd);
    if (ret <= 0) {
        return -1;
    }

    int j = 0;
    for (int i = 0; raw[i] && j < bufsize - 1; i++) {
        if (raw[i] == ':' || raw[i] == '\n' || raw[i] == '\r') {
            continue;
        }
        buf[j++] = raw[i];
    }
    buf[j] = '\0';
    return j > 0 ? 0 : -1;
}

void get_mac_address(char* buf) {
    const char* ifaces[] = {"eth0", "eth1", "wlan0", NULL};

    memset(buf, 0, 128);
    for (int i = 0; ifaces[i]; i++) {
        if (read_mac_from_iface(ifaces[i], buf, 128) == 0) {
            return;
        }
    }
}

int watchdog_start(Client* c) {
    if (c->watchdogFd >= 0) {
        return 0;
    }
    c->watchdogFd = open(WDT_DEV, O_WRONLY | O_CLOEXEC);
    if (c->watchdogFd < 0) {
        perror("[client] open watchdog");
        return -1;
    }
    int timeout = WDT_TIMEOUT_SEC;
    if (ioctl(c->watchdogFd, WDIOC_SETTIMEOUT, &timeout) < 0) {
        perror("[client] set watchdog timeout");
        close(c->watchdogFd);
        c->watchdogFd = -1;
        return -1;
    }

    ioctl(c->watchdogFd, WDIOC_GETTIMEOUT, &timeout);
    printf("[client] watchdog started, timeout=%ds\n", timeout);
    return watchdog_feed(c);
}

int watchdog_feed(Client* c) {
    if (c->watchdogFd < 0) {
        return -1;
    }
    if (ioctl(c->watchdogFd, WDIOC_KEEPALIVE, 0) < 0) {
        perror("[client] feed watchdog");
        return -1;
    }
    return 0;
}

int watchdog_stop(Client* c) {
    if (c->watchdogFd < 0) {
        return 0;
    }
    int flags = WDIOS_DISABLECARD;
    if (ioctl(c->watchdogFd, WDIOC_SETOPTIONS, &flags) < 0) {
        perror("[client] stop watchdog");
        close(c->watchdogFd);
        c->watchdogFd = -1;
        return -1;
    }
    close(c->watchdogFd);
    c->watchdogFd = -1;
    printf("[client] watchdog stopped\n");
    return 0;
}