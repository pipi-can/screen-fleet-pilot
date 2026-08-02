# Screen Fleet Pilot

商业显示设备远程管控系统 — 面向商场、酒店、医院等场景的数字标牌 / 广告屏 / 信息发布屏，提供设备监控、内容推送、定时播放、远程截屏、OTA 固件升级等能力。

```
┌─────────────────────┐       TCP/JSON        ┌─────────────────────┐       TCP/JSON        ┌──────────────────────────────┐
│  screen-fleet-pilot │  ◄──────────────────►  │  screen-fleet-pilot │  ◄──────────────────►  │  screen-fleet-pilot-embedded │
│       -client       │      端口 8000         │       -server       │      端口 8000         │   (device-agent + Qt 播放器)  │
│   Qt 运维管理后台    │                        │   epoll TCP 服务器   │                        │      I.MX6ULL / 模拟器        │
└─────────────────────┘                        └─────────────────────┘                        └──────────────────────────────┘
         │                                              │                                              │
         │ HTTP 上传素材/固件                              │ SQLite 持久化                                 │ HTTP 下载素材/固件
         ▼                                              ▼                                              ▼
   /uploads  /firmwares                          screen_fleet.db                          /var/lib/device-agent/
```

---

## 一、技术选型与架构

### 1.1 服务端（screen-fleet-pilot-server）

| 项目 | 说明 |
|------|------|
| **语言** | C++（C++17 兼容） |
| **编译器** | g++ |
| **并发模型** | 单线程 `epoll` 事件循环 |
| **JSON 库** | json-c |
| **持久化** | SQLite3（设备信息、屏蔽列表、计划任务） |
| **定时调度** | `timerfd` + 最小堆优先队列（`ScheduleMgr`） |
| **监听端口** | `8000` |
| **构建** | `cd screen-fleet-pilot-server && make` → `build/screen-fleet-pilot-server` |

服务端负责维护所有 TCP 长连接，区分 **embedded（终端设备）** 与 **client（管理后台）** 两类节点，完成协议路由、心跳超时检测（30 秒无心跳判离线）、素材 URL 拼装、计划任务调度与 OTA 指令下发。

### 1.2 管理后台（screen-fleet-pilot-client）

| 项目 | 说明 |
|------|------|
| **语言** | C++17 + QML |
| **框架** | Qt 6.5.3（Qt Quick / Quick Controls 2 / Network / Multimedia） |
| **UI 风格** | Fusion 深色主题，无边框窗口 |
| **架构模式** | C++ ViewModel（`NetworkManager`、`DeviceListModel`）+ QML View |
| **构建** | Qt Creator 打开 `screen-fleet-pilot-client.pro`，或 qmake + make |
| **运行平台** | Windows / Linux 桌面（x86_64） |

管理后台通过 `QTcpSocket` 连接服务端，业务逻辑集中在 C++ 层；QML 只负责渲染与交互。素材和固件通过 HTTP 上传到云服务器（`/uploads`、`/firmwares`），推送时只传服务器相对路径。

### 1.3 嵌入式设备端（screen-fleet-pilot-embedded）

设备端采用 **双进程架构**，网络 IO 与 UI 渲染解耦，便于在资源受限的 ARM 板卡上稳定运行：

| 进程 | 语言 | 运行平台 | 职责 |
|------|------|----------|------|
| **device-agent** | C | I.MX6ULL（ARM）/ x86 调试 | TCP 通信、注册/心跳、HTTP 下载、OTA、截屏、看门狗 |
| **screen-fleet-pilot-embedded** | C++ / QML | 同左 | 图片轮播播放、定时任务本地调度、截屏抓图 |

**交叉编译（I.MX6ULL）**

`device-agent` 使用 ARM 交叉工具链静态链接 json-c：

```bash
# 工具链
CC = arm-linux-gnueabihf-gcc

# 依赖库路径（json-c 需预先为 ARM 编译并安装）
LIBDIR = /home/build

cd screen-fleet-pilot-embedded/device-agent
make    # 输出 build/device-agent
```

Qt 播放器 `screen-fleet-pilot-embedded` 需使用 **Buildroot / Yocto SDK** 或厂商提供的 Qt 交叉编译环境，目标平台插件为 `linuxfb`（全屏渲染到 `/dev/fb0`）。开机启动参考 `screen-fleet-pilot-embedded/scripts/rc.local.example`：先启动 Qt 播放器，再启动 device-agent。

| 项目 | 说明 |
|------|------|
| **Qt 版本（播放器）** | Qt 5.x Quick（QML `import QtQuick 2.12`） |
| **进程间通信** | Unix Domain Socket `/tmp/screen_fleet_pilot.sock` + JSON 文件 |
| **本地存储** | `/var/lib/device-agent/content/`（playlist.json、schedule.json、素材缓存） |
| **固件版本** | `DEVICE_VERSION = 1.0.5`（device-agent） |

---

## 二、各端功能实现

### 2.1 服务端

- TCP 长连接管理（`epoll` + 每 FD 读缓冲区，按 `\n` 切分 JSON Lines）
- 设备注册 / 重连（`device_uid` 唯一标识，SQLite 持久化）
- 心跳接收与健康数据更新（CPU 温度、内存、磁盘、设备本地时间戳）
- 心跳超时检测（> 30 秒无心跳，管理后台显示离线）
- 管理后台注册与设备列表查询（`fetch_devices_ack`，合并 DB + 在线实时数据）
- 设备信息编辑转发、设备屏蔽（mask）
- **内容推送**：接收路径列表 → 拼装 HTTP URL → 下发 `push_resources_to_download`
- **定时推送双模式**（见下文第四节）
- 远程截屏请求转发与结果回传
- OTA 固件 MD5 校验与 `ota_update` 下发
- 计划任务持久化 + `timerfd` 到期触发

### 2.2 管理后台

| 页面 | 功能 |
|------|------|
| **设备仪表盘** | 在线/告警/离线/总数统计；分组树；设备搜索；编辑名称/分组；屏蔽设备；远程截屏；跳转推送 |
| **内容推送** | 服务器资源库浏览；按类型筛选；选设备/分组；**立即循环播放** 或 **定时播放** 两种模式 |
| **OTA 升级** | 服务器固件列表；本地上传固件；MD5 校验；选择在线设备；下发升级并显示结果 |

C++ 核心模块：

- `NetworkManager` — TCP 连接、自动重连、协议收发、心跳
- `DeviceListModel` — 设备列表增量合并、分组过滤、告警统计
- `FileUploadManager` — HTTP 断点续传上传素材/固件

### 2.3 嵌入式设备端

**device-agent（C）**

- `epoll` 事件循环 + `timerfd` 定时心跳（5 秒间隔，`HEARTBEAT_INTERVAL_SEC`）
- TCP 注册 / 自动重连
- 接收 `push_resources_to_download` → 多线程 HTTP 下载 → 写 `playlist.json` → 通知 Qt 播放器
- 接收 `push_schedule_playlist` → 写 `schedule.json` → 预下载缺失素材
- 接收 `request_screenshot` → 通知 Qt 抓图 → 上传截图路径给服务端
- 接收 `ota_update` → curl 下载固件 → MD5 校验 → 调用 `ota_update.py` 升级 → 回传 `ota_update_ack`
- 看门狗喂狗（`/dev/watchdog`）

**screen-fleet-pilot-embedded（Qt QML）**

- `ContentPlayer` 监听 Unix Socket，读取 `playlist.json` / `schedule.json`
- QML `Image` 组件轮播显示（10 秒切换，淡入动画）
- 定时任务本地轮询（`SCHEDULE_POLL_MS = 1000`），到点播放计划素材，到期恢复默认播放列表
- 远程截屏：`grabToImage` 抓图并保存

### 2.4 辅助工具

```bash
# Python 设备模拟器（开发 / 压测，无需真机）
python scripts/device_simulator.py --host <SERVER_IP> --port 8000 --count 15

# OTA 固件打包（device-agent + Qt 播放器 → .tgz）
python scripts/pack_firmware.py --version 1.0.6 --changelog "修复xxx"
```

---

## 三、各端内部架构

### 3.1 服务端架构

```
┌─────────────────────────────────────────────────────────────────┐
│                     screen-fleet-pilot-server                    │
│                                                                  │
│  main.cpp                                                        │
│    ├── LogMgr          日志初始化                                 │
│    ├── DatabaseMgr     SQLite 建表 / 设备&任务 CRUD               │
│    ├── ScheduleMgr     timerfd + 最小堆 + 到期 executeTask        │
│    ├── SocketMgr       listen(8000)                              │
│    └── EpollManager    epoll_wait 主循环                          │
│           │                                                      │
│           ├── listen fd     → accept 新连接                       │
│           ├── timerfd       → ScheduleMgr::onTimerExpired()      │
│           ├── client fd     → handleClientMessage()              │
│           │     register / fetch_devices / push / schedule /     │
│           │     screenshot / ota / mask / edit ...               │
│           └── embedded fd   → handleEmbeddedMessage()            │
│                 register / heartbeat / screenshot_data /           │
│                 ota_update_ack / update_info_ack                   │
│                                                                  │
│  数据结构：m_fd2DeviceMap / m_uid2IdMap / m_id2fdMap             │
│  心跳检测：checkTimeout() — 30s 无心跳标记离线                     │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 管理后台架构

```
┌─────────────────────────────────────────────────────────────────┐
│                     screen-fleet-pilot-client                    │
│                                                                  │
│  main.cpp                                                        │
│    QGuiApplication + QQmlApplicationEngine                       │
│    注入 NetworkManager / FileUploadManager 到 QML Context         │
│                                                                  │
│  ┌────────────────── C++ 业务层 ──────────────────────────────┐  │
│  │  NetworkManager (QTcpSocket + QTimer)                       │  │
│  │    connect / register / heartbeat / 协议编解码 / 自动重连     │  │
│  │  DeviceListModel (QAbstractListModel)                       │  │
│  │    增量合并 / 分组 / 搜索 / 告警统计 / 在线列表              │  │
│  │  FileUploadManager (QNetworkAccessManager)                  │  │
│  │    HTTP 上传素材 → /uploads   固件 → /firmwares             │  │
│  └─────────────────────────────────────────────────────────────┘  │
│                          │ signals / Q_PROPERTY                    │
│  ┌────────────────── QML 视图层 ──────────────────────────────┐  │
│  │  main.qml          无边框窗口 + Tab 切换 + 截屏弹窗          │  │
│  │  DashboardPage     分组树 + 设备列表 + 编辑弹窗              │  │
│  │  ContentPushPage   资源库 + 目标选择 + 立即/定时推送         │  │
│  │  OTAPage           固件列表 + 设备选择 + 升级进度            │  │
│  └─────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 3.3 嵌入式设备端架构

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        I.MX6ULL 终端设备                                  │
│                                                                           │
│  ┌──────────────────────── device-agent (C) ──────────────────────────┐  │
│  │  epoll 主循环                                                        │  │
│  │    ├── serverFd (TCP)     → 注册 / 心跳 / 指令接收                   │  │
│  │    ├── heartbeatTimerFd   → timerfd 每 5s 发 heartbeat             │  │
│  │    └── qtFd (Unix Socket) → 与 Qt 播放器通信                         │  │
│  │                                                                      │  │
│  │  指令处理：                                                           │  │
│  │    push_resources_to_download → pthread 下载 → playlist.json       │  │
│  │    push_schedule_playlist     → schedule.json + 预下载               │  │
│  │    request_screenshot         → 通知 Qt 抓图                         │  │
│  │    ota_update                 → curl 下载 → MD5 → ota_update.py    │  │
│  └──────────────────────────────┬───────────────────────────────────────┘  │
│                                 │ Unix Socket + JSON 文件                  │
│                                 │ /tmp/screen_fleet_pilot.sock            │
│                                 │ /var/lib/device-agent/content/          │
│  ┌──────────────────────────────▼───────────────────────────────────────┐  │
│  │  screen-fleet-pilot-embedded (Qt QML)                                │  │
│  │    ContentPlayer                                                     │  │
│  │      ├── 读 playlist.json  → 立即轮播                                 │  │
│  │      ├── 读 schedule.json  → 本地定时播放（1s 轮询）                  │  │
│  │      └── grabToImage       → 远程截屏                                 │  │
│  │    main.qml                                                          │  │
│  │      Image { source: player.currentImage }  ← linuxfb → /dev/fb0    │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 四、三端通信协议

### 4.0 协议格式

三端统一使用 **JSON Lines** 协议：一行一个 JSON 对象，以 `\n` 结尾。

```json
{
  "source": "client | server | embedded",
  "cmd": "指令名",
  "seq": 1,
  "timestamp": 1718347200,
  "device_id": 3,
  "params": { }
}
```

| 字段 | 说明 |
|------|------|
| `source` | 消息来源端 |
| `cmd` | 指令名称 |
| `seq` | 序列号（可选，用于日志追踪） |
| `device_id` | 服务端分配的连接 ID（注册后使用） |
| `params` | 指令参数，结构因指令而异 |

TCP 是流式传输，接收方必须在应用层按 `\n` 切分包（粘包 / 半包处理）。

---

### 4.1 心跳保活

**目的**：终端定期上报健康数据；服务端更新 `lastUploadTime`；管理后台定时 `fetch_devices` 刷新列表；超过 30 秒无心跳判离线。

**流程**：

```
embedded ──heartbeat(每5s)──► server ──更新内存中的温度/内存/磁盘──►
client ──fetch_devices(每5s)──► server ──fetch_devices_ack(含online字段)──► client 刷新 UI
```

**① 终端 → 服务端：heartbeat**

```json
{
  "source": "embedded",
  "cmd": "heartbeat",
  "seq": 42,
  "device_id": 3,
  "params": {
    "cpu_temp": "52",
    "mem_usage": 34,
    "disk_free_mb": 5200,
    "current_content": "大厅屏A",
    "timestamp": 1718347200
  }
}
```

> `cpu_temp` 来自 `/sys/class/thermal`；`timestamp` 为设备本地 Unix 时间，服务端用于判断时钟是否可信（定时推送双模式）。

**② 管理后台 → 服务端：fetch_devices**

```json
{
  "source": "client",
  "cmd": "fetch_devices",
  "seq": 10,
  "timestamp": 1718347205,
  "params": {}
}
```

**③ 服务端 → 管理后台：fetch_devices_ack**

```json
{
  "source": "server",
  "cmd": "fetch_devices_ack",
  "seq": 0,
  "params": {
    "devices": [
      {
        "device_uid": "MAC-XX-XX-XX-XX-XX-XX",
        "name": "大厅屏A",
        "group": "一层",
        "id": 3,
        "version": "1.0.5",
        "temperature": "52",
        "mem_usage": 34,
        "disk_free_mb": 5200,
        "online": true
      },
      {
        "device_uid": "SIM-餐厅屏001-08",
        "name": "餐厅屏001",
        "group": "餐厅",
        "id": -1,
        "version": "",
        "temperature": "",
        "mem_usage": 0,
        "disk_free_mb": 0,
        "online": false
      }
    ]
  }
}
```

**④ 管理后台 → 服务端：heartbeat（保活管理端连接）**

```json
{
  "source": "client",
  "cmd": "heartbeat",
  "timestamp": 1718347210,
  "device_id": 1,
  "params": {}
}
```

---

### 4.2 内容推送（立即播放）

**目的**：运维在管理后台选择服务器资源库中的图片，推送到指定设备，设备下载后立即轮播。

**流程**：

```
client ──request_push_content_to_embedded──► server
server ──push_resources_to_download(urls)──► embedded (每台目标设备)
embedded ──HTTP下载素材──► 写 playlist.json ──► 通知 Qt 播放器切换画面
```

**① 管理后台 → 服务端**

```json
{
  "source": "client",
  "cmd": "request_push_content_to_embedded",
  "seq": 20,
  "timestamp": 1718347300,
  "params": {
    "device_uids": [
      "MAC-XX-XX-XX-XX-XX-XX",
      "SIM-大厅屏B-02"
    ],
    "paths": [
      "/uploads/促销海报.jpg",
      "/uploads/欢迎页.png"
    ]
  }
}
```

**② 服务端 → 终端（每台设备一条）**

服务端将相对路径拼成完整 URL（`http://<SERVER_IP>/uploads/...`）后下发：

```json
{
  "source": "server",
  "cmd": "push_resources_to_download",
  "seq": 0,
  "params": {
    "paths": [
      "http://8.136.113.168/uploads/促销海报.jpg",
      "http://8.136.113.168/uploads/欢迎页.png"
    ]
  }
}
```

**③ 终端本地处理（无 JSON 回包）**

1. `device-agent` 启动下载线程，将文件保存到 `/var/lib/device-agent/content/`
2. 写入 `playlist.json`：

```json
{
  "cmd": "content_ready",
  "paths": [
    "/var/lib/device-agent/content/促销海报.jpg",
    "/var/lib/device-agent/content/欢迎页.png"
  ]
}
```

3. `ContentPlayer` 检测到新 playlist，QML `Image` 开始 10 秒轮播

---

### 4.3 定时推送（双模式）

管理后台 **内容推送页** 支持两种播放模式（`playMode`）：

| 模式 | UI 选项 | 客户端指令 | 说明 |
|------|---------|-----------|------|
| **立即循环** | `loop` | `request_push_content_to_embedded` | 见 4.2，立即下载并播放 |
| **定时播放** | `schedule` | `request_schedule_push` | 在指定日期时间播放，持续指定秒数 |

定时推送在服务端进一步分为 **双路径**，根据终端时钟是否可信自动选择：

```
                    request_schedule_push
                            │
                            ▼
              ┌──── 设备 timestamp 与服务器差值 ≤ 60s？ ────┐
              │ 是（时钟可信）                                  │ 否（时钟不可信）
              ▼                                                ▼
   push_schedule_playlist                           ScheduleMgr 入队
   直接下发到终端                                     (SQLite + 最小堆 + timerfd)
   终端本地定时播放                                   到期 executeTask
                                                    → push_resources_to_download
                                                      （到点由服务器触发立即推送）
```

**① 管理后台 → 服务端：request_schedule_push**

```json
{
  "source": "client",
  "cmd": "request_schedule_push",
  "seq": 30,
  "timestamp": 1718347400,
  "params": {
    "device_uids": ["MAC-XX-XX-XX-XX-XX-XX"],
    "paths": ["/uploads/早市促销.jpg"],
    "schedule_date": "2026-07-08",
    "schedule_time": "08:00",
    "duration_sec": 300
  }
}
```

**② 模式 A — 终端时钟可信：服务端 → 终端 `push_schedule_playlist`**

```json
{
  "source": "server",
  "cmd": "push_schedule_playlist",
  "seq": 0,
  "params": {
    "paths": [
      "http://8.136.113.168/uploads/早市促销.jpg"
    ],
    "schedule_date": "2026-07-08",
    "schedule_time": "08:00",
    "duration_sec": 300
  }
}
```

终端写入 `schedule.json`，`ContentPlayer` 每秒轮询，到 `08:00` 播放促销图，300 秒后恢复默认 playlist。

**③ 模式 B — 终端时钟不可信：服务端本地调度**

任务写入 SQLite，加入最小堆；`timerfd` 在 `2026-07-08 08:00:00` 触发后，`ScheduleMgr::executeTask()` 调用 `pushResourcesToEmbedded()`，下发与 4.2 相同的 `push_resources_to_download` 包。

> 双模式的设计动机：嵌入式设备可能未同步 NTP，本地定时不可靠；此时由服务端统一计时，到点代为推送。

---

### 4.4 OTA 固件升级

**目的**：运维上传新固件包到服务器，选择在线终端，远程升级 `device-agent` + Qt 播放器。

**流程**：

```
client ──HTTP上传──► /firmwares/xxx.tgz
client ──request_check_firmware(md5)──► server ──check_firmware_ack──► client
client ──request_ota_update──► server ──ota_update(path,md5)──► embedded
embedded ──curl下载──► MD5校验 ──► ota_update.py 解压替换 ──► ota_update_ack──► server ──► client
```

**① 管理后台 → 服务端：request_ota_update**

```json
{
  "source": "client",
  "cmd": "request_ota_update",
  "seq": 50,
  "timestamp": 1718347600,
  "params": {
    "device_uids": [
      "MAC-XX-XX-XX-XX-XX-XX"
    ],
    "path": "/firmwares/screen-fleet-pilot-firmware-v1.0.6.tgz",
    "device_uid": "CLIENT-UUID-管理后台唯一ID"
  }
}
```

> `params.device_uid` 是**管理后台**的 UID（非终端 UID），用于服务端将升级结果路由回正确的客户端。

**② 服务端 → 终端：ota_update**

服务端读取固件文件并计算 MD5 后下发：

```json
{
  "source": "server",
  "cmd": "ota_update",
  "seq": 0,
  "params": {
    "path": "/firmwares/screen-fleet-pilot-firmware-v1.0.6.tgz",
    "md5": "a1b2c3d4e5f6789012345678901234ab",
    "device_uid": "CLIENT-UUID-管理后台唯一ID"
  }
}
```

**③ 终端 → 服务端：ota_update_ack**

```json
{
  "source": "embedded",
  "cmd": "ota_update_ack",
  "seq": 7,
  "params": {
    "code": 0,
    "result": 1,
    "msg": "upgrade ok",
    "path": "/firmwares/screen-fleet-pilot-firmware-v1.0.6.tgz",
    "local_path": "/var/lib/device-agent/firmware/screen-fleet-pilot-firmware-v1.0.6.tgz",
    "device_uid": "CLIENT-UUID-管理后台唯一ID"
  }
}
```

| 字段 | 含义 |
|------|------|
| `code` | `0` 成功，`-1` 失败 |
| `result` | `1` 升级成功，`0` 仅下载成功或失败 |
| `device_uid` | 回传给发起升级的管理后台，用于 UI 更新 |

**④ 服务端 → 管理后台：ota_update_ack（转发）**

```json
{
  "source": "server",
  "cmd": "ota_update_ack",
  "seq": 0,
  "params": {
    "code": 0,
    "result": 1,
    "msg": "upgrade ok",
    "path": "/firmwares/screen-fleet-pilot-firmware-v1.0.6.tgz"
  }
}
```

**固件包结构**（`scripts/pack_firmware.py` 生成）：

```
screen-fleet-pilot-firmware-v1.0.6.tgz
├── device-agent                  # ARM 可执行文件
├── screen-fleet-pilot-embedded   # Qt 播放器
└── README.json                   # version / changelog / 文件清单
```

---

## 五、快速启动

### 5.1 启动服务端

```bash
cd screen-fleet-pilot-server
make
./build/screen-fleet-pilot-server
# 监听 0.0.0.0:8000
```

### 5.2 启动管理后台

```bash
# Qt Creator 打开 screen-fleet-pilot-client/screen-fleet-pilot-client.pro
# 修改 main.qml 中 connectToServer 的 IP/端口后运行
```

### 5.3 启动模拟设备（无需 ARM 板）

```bash
python scripts/device_simulator.py --host 127.0.0.1 --port 8000 --count 5
```

### 5.4 启动 I.MX6ULL 真机

```bash
# 1. 交叉编译 device-agent 并部署到板子
# 2. 交叉编译 Qt 播放器，使用 -platform linuxfb:fb=/dev/fb0
# 3. 参考 scripts/rc.local.example 配置开机自启
```

---

## 六、项目目录

```
screen-fleet-pilot/
├── README.md                          # 本文件
├── screen-fleet-pilot-server/         # TCP 服务端（epoll + SQLite + timerfd）
├── screen-fleet-pilot-client/         # Qt 管理后台
├── screen-fleet-pilot-embedded/       # 嵌入式设备端
│   ├── device-agent/                  # C 网络代理（ARM 交叉编译）
│   ├── interfaces/contentplayer.cpp   # Qt 播放引擎
│   ├── main.qml                       # 全屏图片播放器
│   └── scripts/rc.local.example       # 真机开机启动脚本
└── scripts/
    ├── device_simulator.py            # Python 多设备模拟器
    └── pack_firmware.py               # OTA 固件打包工具
```

---

## 七、依赖汇总

| 组件 | 依赖 |
|------|------|
| server | g++, json-c, sqlite3, pthread |
| client | Qt 6.5.3（Quick, Network, Multimedia） |
| device-agent | arm-linux-gnueabihf-gcc, json-c（静态）, curl |
| Qt 播放器 | Qt 5.x Quick, linuxfb 平台插件 |
| 工具 | Python 3 |

---

## License

本项目为个人学习与作品集项目，按需使用。
