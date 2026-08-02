#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <json-c/json.h>
#include <sys/timerfd.h>
#include <pthread.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <linux/watchdog.h>
#include <sys/types.h>
#define HEARTBEAT_INTERVAL_SEC  5
#define RECONNECT_INTERVAL_SEC  5
#define CONTENT_DIR             "/var/lib/device-agent/content"
#define PLAYLIST_JSON_PATH      CONTENT_DIR "/playlist.json"
#define SCHEDULE_JSON_PATH      CONTENT_DIR "/schedule.json"
#define SOCKET_PATH             "/tmp/screen_fleet_pilot.sock"
#define SCREENSHOT_URL_PREFIX   "/screenshots"
#define FIRMWARE_DIR            "/var/lib/device-agent/firmware"
#define OTA_SCRIPT_PATH         "./ota_update.py"

#define WDT_DEV                 "/dev/watchdog"
#define WDT_TIMEOUT_SEC         20
#define WDT_FEED_INTERVAL_SEC   5

extern const char* DEVICE_VERSION;

// ── 读缓冲区（解决 TCP 粘包/半包） ──
typedef struct {
    char data[8192];
    int  len;
} FdBuffer;

// ── 客户端上下文 ──
typedef struct {
    int  epfd;           // epoll fd
    int  serverFd;       // 连服务端的 socket
    int  running;
    int  heartbeatTimerFd;
    int  watchdogFd;
    char deviceName[64];
    char deviceGroup[64];
    char deviceUid[128];
    char deviceVersion[32];
    char serverIp[64];

    int  deviceId;       // 服务端分配
    int  registered;     // 0=未注册, 1=已注册
    int  heartbeatSeq;
    int  msgSeq;

    FdBuffer recvBuf;    // 接收缓冲区
    FdBuffer qtRecvBuf;  // Qt 长连接读缓冲
} Client;

typedef struct download_context {
    char** urls;
    int count;
    int notify_qt;
} download_context;

void* batch_download_thread(void* arg);
int  notify_content(char** contents, int count);
// ── API ──
int  client_init(Client* c, const char* ip, int port,
                 const char* name, const char* group, const char* uid);
void client_run(Client* c);
void client_destroy(Client* c);

// ── 内部 ──
void client_send_json(Client* c, struct json_object* obj);
void client_send_register(Client* c);
void client_send_heartbeat(Client* c);
void client_on_recv(Client* c);
void client_handle_message(Client* c, const char* jsonStr);
void client_handle_register_ack(Client* c, struct json_object* params);
void client_handle_update_embedded_info(Client* c, struct json_object* params);
void client_handle_push_resources_to_download(Client* c, struct json_object* params);
void client_handle_push_schedule_playlist(Client* c, struct json_object* params);
void client_handle_request_screenshot(Client* c, struct json_object* params);
void client_handle_ota_update(Client* c, struct json_object* params);

void get_cpu_temp(char* buf);
void get_mem_usage(int* buf);
void get_disk_free_mb(int* buf);
void get_mac_address(char* buf);

int  watchdog_start(Client* c);
int  watchdog_feed(Client* c);
int  watchdog_stop(Client* c);
#endif 