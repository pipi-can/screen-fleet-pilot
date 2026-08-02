#!/usr/bin/env python3
"""
嵌入式设备模拟器 — 用于测试 screen-fleet-pilot-client 管理平台

每个模拟设备 = 一条独立 TCP 连接，流程：
  1. TCP 连接服务器
  2. 发送 register（随机设备名 / 分组 / 指标）
  3. 接收 register_ack
  4. 每 5s 发送 status 心跳（温度、内存、磁盘动态变化）
  5. 断开自动重连

用法：
  python device_simulator.py --host 8.136.113.168 --port 8000 --count 15
"""

import socket
import json
import time
import random
import threading
import argparse
import sys
import signal
import select
from typing import Optional, Dict

# ═══════════════════════════════════════
# 设备模板数据
# ═══════════════════════════════════════

DEVICE_TEMPLATES = [
    # (名称, 分组, 版本, 基础温度, 基础内存, 基础磁盘MB)
    ("大厅屏A",   "一层", "1.0.3", 52, 34, 5200),
    ("大厅屏B",   "一层", "1.0.3", 46, 31, 7500),
    ("电梯屏L",   "一层", "1.0.3", 48, 28, 6800),
    ("电梯屏R",   "一层", "1.0.3", 47, 29, 6100),
    ("前台屏",    "大厅", "1.0.3", 44, 26, 8100),
    ("休息区屏",  "大厅", "1.0.3", 43, 22, 9200),
    ("导引屏A",   "大厅", "1.0.3", 45, 30, 5700),
    ("餐厅屏001", "餐厅", "1.0.3", 71, 62, 3100),  # 高温告警
    ("餐厅屏002", "餐厅", "1.0.3", 55, 38, 4800),
    ("餐厅屏003", "餐厅", "1.0.3", 49, 33, 6300),
    ("车库屏A",   "地库", "1.0.3", 40, 25, 12000),
    ("车库屏B",   "地库", "1.0.3", 39, 21, 11500),
    ("广告屏01",  "三层", "1.0.3", 51, 36, 4400),
    ("广告屏02",  "三层", "1.0.3", 53, 40, 3900),
    ("广告屏03",  "三层", "1.0.3", 50, 35, 4600),
    ("VIP屏",     "VIP区", "1.0.3", 42, 27, 10500),
    ("指示屏N",   "一层", "1.0.4", 45, 24, 7200),
    ("水牌屏",    "大厅", "1.0.3", 41, 23, 8600),
    ("取餐屏",    "餐厅", "1.0.3", 58, 42, 3500),
    ("车位屏A",   "地库", "1.0.3", 38, 20, 11000),
]

SEQS = {}          # thread_id → seq counter
STOP_FLAG = False  # graceful shutdown


def make_msg(source: str, cmd: str, seq: int, params: Optional[Dict] = None) -> str:
    """构造一行 JSONL 消息"""
    obj = {
        "source": source,
        "cmd": cmd,
        "seq": seq,
        "timestamp": int(time.time()),
    }
    if params is not None:
        obj["params"] = params
    return json.dumps(obj, ensure_ascii=False) + "\n"


def send_line(sock: socket.socket, data: str):
    """发送一行，出错不抛（连接断开由主循环处理）"""
    try:
        sock.sendall(data.encode("utf-8"))
    except (BrokenPipeError, ConnectionResetError, OSError):
        pass


def recv_all(sock: socket.socket) -> Optional[bytes]:
    """非阻塞读一次：有数据返回 bytes，无数据返回 b""，连接断开返回 None"""
    try:
        sock.settimeout(0)
        data = sock.recv(4096)
        if not data:
            return None
        return data
    except (BlockingIOError, socket.timeout):
        return b""
    except OSError:
        return None


def extract_lines(buf: bytearray):
    """从缓冲区中提取所有完整行（\n 分隔），返回行列表"""
    lines = []
    while True:
        idx = buf.find(b"\n")
        if idx < 0:
            break
        line = buf[:idx].decode("utf-8", errors="replace").strip()
        del buf[:idx + 1]
        if line:
            lines.append(line)
    return lines


def recv_line(sock: socket.socket, timeout: float = 2.0) -> Optional[str]:
    """兼容旧代码：阻塞读一行，内部用 recv_all + extract_lines"""
    sock.settimeout(timeout)
    buf = bytearray()
    try:
        while True:
            data = recv_all(sock)
            if data is None:
                return None
            if data:
                buf.extend(data)
                lines = extract_lines(buf)
                if lines:
                    return lines[0]
    except socket.timeout:
        return None
    except OSError:
        return None


def simulate_device(idx: int, host: str, port: int,
                    name: str, group: str, version: str,
                    base_temp: int, base_mem: int, base_disk: int):
    """
    单个设备模拟线程
    """
    tid = threading.get_ident()
    sock = None

    while not STOP_FLAG:
        try:
            # ── 建立 TCP 连接 ──
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            sock.connect((host, port))
            SEQS[tid] = 0
            print(f"[设备 {idx:02d}] ✅ 已连接 — {name} | {group}")

            # ── 发送注册 ──
            SEQS[tid] += 1
            send_line(sock, make_msg("embedded", "register", SEQS[tid], {
                "name": name,
                "group": group,
                "version": version,
                "device_uid": f"SIM-{name}-{idx:02d}",
            }))

            # ── 读 register_ack ──
            ack = recv_line(sock, timeout=3.0)
            if ack:
                try:
                    ack_obj = json.loads(ack)
                    if ack_obj.get("cmd") == "register_ack":
                        params = ack_obj.get("params", {})
                        if params.get("code") == 0:
                            device_id = params.get("device_id", "?")
                            print(f"[设备 {idx:02d}] 🆔 注册成功 device_id={device_id} — {name}")
                        else:
                            print(f"[设备 {idx:02d}] ❌ 注册失败: {params.get('msg', ack)}")
                except json.JSONDecodeError:
                    print(f"[设备 {idx:02d}] ⚠ 无法解析注册响应: {ack[:60]}")

            # ── 心跳循环：select 监听 + 定时心跳，零盲等 ──
            sock.setblocking(False)
            buf = bytearray()
            heartbeat_interval = random.uniform(4.5, 5.5)
            last_heartbeat = time.time()

            while not STOP_FLAG:
                # select 等待数据或超时 1s
                try:
                    readable, _, _ = select.select([sock], [], [], 1.0)
                except (ValueError, OSError):
                    break

                now = time.time()

                # ── 处理服务器下发的指令 ──
                if readable:
                    data = recv_all(sock)
                    if data is None:
                        break  # 连接断开
                    if data:
                        buf.extend(data)
                        for line in extract_lines(buf):
                            try:
                                msg = json.loads(line)
                                cmd = msg.get("cmd", "")
                                params = msg.get("params", {})
                                print(f"[设备 {idx:02d}] 📩 收到指令: {cmd} — {name}")

                                if cmd == "update_embedded_info":
                                    new_name = params.get("name", name)
                                    new_group = params.get("group", group)
                                    senderFd = params.get("sender")
                                    print(f"[设备 {idx:02d}] ✏️ 改名: {name} → {new_name} / {group} → {new_group}")
                                    name = new_name
                                    group = new_group
                                    SEQS[tid] += 1
                                    send_line(sock, make_msg("embedded", "update_info_ack", SEQS[tid], {
                                        "sender": senderFd,
                                        "group": group,
                                        "name": name,
                                        "msg": "ok",
                                    }))

                                elif cmd == "content_push":
                                    ctype = params.get("type", "?")
                                    print(f"[设备 {idx:02d}] 🖼 收到内容推送: {ctype} — {name}")
                                    SEQS[tid] += 1
                                    send_line(sock, make_msg("embedded", "cmd_ack", SEQS[tid], {
                                        "cmd": cmd,
                                        "code": 0,
                                        "msg": "ok",
                                    }))

                                elif cmd == "screenshot_request":
                                    print(f"[设备 {idx:02d}] 📸 截屏请求 — {name}")
                                    SEQS[tid] += 1
                                    send_line(sock, make_msg("embedded", "screenshot_data", SEQS[tid], {
                                        "device_name": name,
                                        "format": "png",
                                        "width": 800,
                                        "height": 480,
                                        "data_base64": "iVBORw0KGgo=FAKE_SCREENSHOT_DATA",
                                    }))

                            except json.JSONDecodeError:
                                pass

                # ── 到时间就发心跳 ──
                if now - last_heartbeat >= heartbeat_interval:
                    temp = base_temp + random.randint(-3, 3)
                    mem  = max(5, min(99, base_mem + random.randint(-5, 5)))
                    disk = max(100, base_disk - random.randint(0, 200))
                    SEQS[tid] += 1
                    send_line(sock, make_msg("embedded", "heartbeat", SEQS[tid], {
                        "cpu_temp":     temp,
                        "mem_usage":    mem,
                        "disk_free_mb": disk,
                    }))
                    last_heartbeat = now
                    heartbeat_interval = random.uniform(4.5, 5.5)

        except (ConnectionRefusedError, OSError) as e:
            print(f"[设备 {idx:02d}] 🔴 连接失败: {e} — {name}")
        finally:
            if sock:
                try:
                    sock.close()
                except OSError:
                    pass
                sock = None

        if not STOP_FLAG:
            delay = random.uniform(2.0, 8.0)
            print(f"[设备 {idx:02d}] 🔄 {delay:.0f}s 后重连 — {name}")
            time.sleep(delay)


def main():
    global STOP_FLAG

    parser = argparse.ArgumentParser(description="嵌入式设备模拟器")
    parser.add_argument("--host", default="8.136.113.168", help="服务器地址")
    parser.add_argument("--port", type=int, default=8000, help="服务器端口")
    parser.add_argument("--count", type=int, default=15, help="模拟设备数量")
    parser.add_argument("--delay", type=float, default=0.3,
                        help="设备启动间隔（秒），避免瞬间大量连接")
    args = parser.parse_args()

    # 信号处理
    def shutdown(sig, frame):
        global STOP_FLAG
        print("\n⏳ 正在关闭所有模拟设备...")
        STOP_FLAG = True
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    print(f"🚀 启动 {args.count} 个模拟设备 → {args.host}:{args.port}")
    print(f"   模板池: {len(DEVICE_TEMPLATES)} 种设备")
    print("=" * 55)

    threads = []
    for i in range(args.count):
        tpl = DEVICE_TEMPLATES[i % len(DEVICE_TEMPLATES)]

        # 给同模板的设备加编号后缀区分
        base_name = tpl[0]
        if args.count > len(DEVICE_TEMPLATES):
            suffix = (i // len(DEVICE_TEMPLATES)) + 1
            if suffix > 1:
                base_name = f"{tpl[0]}#{suffix}"

        name, group, version, base_temp, base_mem, base_disk = (
            base_name, tpl[1], tpl[2], tpl[3], tpl[4], tpl[5]
        )

        t = threading.Thread(
            target=simulate_device,
            args=(i + 1, args.host, args.port, name, group, version,
                  base_temp, base_mem, base_disk),
            daemon=True,
        )
        t.start()
        threads.append(t)
        time.sleep(args.delay)

    print(f"✅ 全部 {args.count} 个设备已启动，按 Ctrl+C 停止")
    print("=" * 55)

    # 保持主线程存活
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

