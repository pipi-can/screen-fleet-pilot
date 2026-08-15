#ifndef GLOBAL_DEF_H
#define GLOBAL_DEF_H

#define SERVER_HOST             "8.136.113.168"
#define SERVER_PORT             8000
#define MAX_CONNECT_TIME        20
#define DEVICE_MESSAGE_PATH     "/var/lib/device-agent/device_message.json"
#define HEARTBEAT_INTERVAL      5

#define CONTENT_DIR             "/var/lib/device-agent/content"
#define PLAYLIST_JSON_PATH      CONTENT_DIR "/playlist.json"
#define SCHEDULE_JSON_PATH      CONTENT_DIR "/schedule.json"
#define SOCKET_PATH             "/tmp/screen_fleet_pilot.sock"
#define SCREENSHOT_URL_PREFIX   "/screenshots"
#define FIRMWARE_DIR            "/var/lib/device-agent/firmware"
#define OTA_SCRIPT_PATH         "./scripts/ota_update.py"

#endif
