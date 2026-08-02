#!/usr/bin/env python3
"""
screen-fleet-pilot 嵌入式设备压力测试脚本（asyncio）

与 device-agent 协议对齐，用于分档压测 TCP 服务端连接容量与心跳稳定性。
日常开发请继续使用 device_simulator.py；本脚本专注 soak / ramp 压测与报告输出。

用法示例：
  # 单档浸泡：200 台 × 30 分钟
  python stress_device.py --host 127.0.0.1 --port 8000 --mode soak --count 200 --duration 1800

  # 自动分档：L1(20) → L2(50) → L3(200)
  python stress_device.py --host 127.0.0.1 --port 8000 --mode ramp

  # 自定义分档
  python stress_device.py --mode ramp --stages "20:300,50:900,200:1800"
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import random
import signal
import sys
import time
from dataclasses import dataclass, field, asdict
from datetime import datetime
from typing import Any, Dict, List, Optional

# ═══════════════════════════════════════════════════════════════
# 默认分档：L1 冒烟 → L2 常规 → L3 目标（200 连接）
# ═══════════════════════════════════════════════════════════════

DEFAULT_STAGES = [
    {"name": "L1", "count": 20, "duration_sec": 300},
    {"name": "L2", "count": 50, "duration_sec": 900},
    {"name": "L3", "count": 200, "duration_sec": 1800},
]

STAGE_COOLDOWN_SEC = 30
HEARTBEAT_INTERVAL_SEC = 5.0
DEVICE_VERSION = "1.0.5"

DEVICE_TEMPLATES = [
    ("大厅屏A", "一层", 52, 34, 5200),
    ("大厅屏B", "一层", 46, 31, 7500),
    ("电梯屏L", "一层", 48, 28, 6800),
    ("餐厅屏001", "餐厅", 71, 62, 3100),
    ("车库屏A", "地库", 40, 25, 12000),
    ("广告屏01", "三层", 51, 36, 4400),
    ("VIP屏", "VIP区", 42, 27, 10500),
    ("前台屏", "大厅", 44, 26, 8100),
]

# 全局停止标志（Ctrl+C）
STOP_REQUESTED = False


def make_msg(
    source: str,
    cmd: str,
    seq: int,
    params: Optional[Dict[str, Any]] = None,
    device_id: Optional[int] = None,
) -> str:
    """
    @brief: 构造一行 JSON Lines 消息，格式与三端协议一致
    @param: source，消息来源 embedded/server/client
    @param: cmd，指令名
    @param: seq，序列号
    @param: params，可选参数字典
    @param: device_id，注册后心跳需携带的服务端分配 ID
    @retval: 以 \\n 结尾的 JSON 字符串
    """
    obj: Dict[str, Any] = {
        "source": source,
        "cmd": cmd,
        "seq": seq,
        "timestamp": int(time.time()),
    }
    if device_id is not None:
        obj["device_id"] = device_id
    if params is not None:
        obj["params"] = params
    return json.dumps(obj, ensure_ascii=False) + "\n"


@dataclass
class Stats:
    """
    @brief: 单档压测的全局计数器（asyncio 单线程事件循环内更新，无需加锁）
    """

    connect_attempts: int = 0
    connect_ok: int = 0
    register_ok: int = 0
    register_fail: int = 0
    heartbeats_sent: int = 0
    unexpected_disconnects: int = 0
    cmd_received: Dict[str, int] = field(default_factory=dict)

    def inc_cmd(self, cmd: str) -> None:
        self.cmd_received[cmd] = self.cmd_received.get(cmd, 0) + 1

    def snapshot(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class StageResult:
    """
    @brief: 单档压测结果，用于终端输出与 JSON 报告
    """

    name: str
    target_count: int
    duration_sec: int
    started_at: str
    ended_at: str
    elapsed_sec: float
    stats: Dict[str, Any]
    registered_at_end: int
    passed: bool
    fail_reason: str = ""


def parse_stages(text: str) -> List[Dict[str, Any]]:
    """
    @brief: 解析 --stages 参数字符串，格式 count:seconds,count:seconds
    @param: text，用户传入的分档配置
    @retval: 分档列表，每项含 name/count/duration_sec
    """
    stages: List[Dict[str, Any]] = []
    parts = [p.strip() for p in text.split(",") if p.strip()]
    for i, part in enumerate(parts):
        if ":" not in part:
            raise ValueError(f"invalid stage '{part}', use count:seconds")
        count_s, dur_s = part.split(":", 1)
        stages.append({
            "name": f"S{i + 1}",
            "count": int(count_s),
            "duration_sec": int(dur_s),
        })
    return stages


def device_profile(index: int) -> Dict[str, Any]:
    """
    @brief: 根据序号生成模拟设备名称、分组与健康数据基线
    @param: index，设备序号（从 0 开始）
    @retval: 包含 name/group/base_temp 等字段的字典
    """
    tpl = DEVICE_TEMPLATES[index % len(DEVICE_TEMPLATES)]
    name, group, base_temp, base_mem, base_disk = tpl
    batch = index // len(DEVICE_TEMPLATES)
    if batch > 0:
        name = f"{name}#{batch + 1}"
    return {
        "name": name,
        "group": group,
        "base_temp": base_temp,
        "base_mem": base_mem,
        "base_disk": base_disk,
        "device_uid": f"STRESS-{name}-{index:04d}",
    }


class EmbeddedSimulator:
    """
    @brief: 单个嵌入式终端模拟器，维护一条 TCP 长连接及心跳
    """

    def __init__(
        self,
        index: int,
        host: str,
        port: int,
        stats: Stats,
        verbose: bool,
    ) -> None:
        self.index = index
        self.host = host
        self.port = port
        self.stats = stats
        self.verbose = verbose
        self.profile = device_profile(index)
        self.seq = 0
        self.device_id: Optional[int] = None
        self.registered = False
        self._name = self.profile["name"]
        self._group = self.profile["group"]
        self._reader: Optional[asyncio.StreamReader] = None
        self._writer: Optional[asyncio.StreamWriter] = None
        self._tasks: List[asyncio.Task] = []

    async def send_line(self, line: str) -> None:
        """@brief: 发送一行 JSON 到服务端"""
        if not self._writer:
            return
        self._writer.write(line.encode("utf-8"))
        await self._writer.drain()

    async def send_register(self) -> None:
        """@brief: 发送 register，字段与 device-agent/client.c 一致"""
        self.seq += 1
        params = {
            "name": self.profile["name"],
            "group": self.profile["group"],
            "version": DEVICE_VERSION,
            "device_uid": self.profile["device_uid"],
        }
        await self.send_line(make_msg("embedded", "register", self.seq, params))

    async def send_heartbeat(self) -> None:
        """@brief: 发送 heartbeat，cpu_temp 为字符串，携带 device_id"""
        if not self.registered or self.device_id is None:
            return
        self.seq += 1
        temp = self.profile["base_temp"] + random.randint(-3, 3)
        mem = max(5, min(99, self.profile["base_mem"] + random.randint(-5, 5)))
        disk = max(100, self.profile["base_disk"] - random.randint(0, 200))
        params = {
            "cpu_temp": str(temp),
            "mem_usage": mem,
            "disk_free_mb": disk,
            "current_content": self._name,
            "timestamp": int(time.time()),
        }
        await self.send_line(
            make_msg("embedded", "heartbeat", self.seq, params, self.device_id)
        )
        self.stats.heartbeats_sent += 1

    async def handle_server_cmd(self, msg: Dict[str, Any]) -> None:
        """
        @brief: 处理服务端下发的指令，压测场景下仅计数并回必要 ack
        @param: msg，解析后的 JSON 对象
        """
        cmd = msg.get("cmd", "")
        params = msg.get("params") or {}
        self.stats.inc_cmd(cmd)

        if cmd == "register_ack":
            code = params.get("code", -1)
            if code == 0:
                self.device_id = params.get("device_id")
                self.registered = True
                self.stats.register_ok += 1
                if self.verbose:
                    print(f"[#{self.index:04d}] register ok device_id={self.device_id}")
            else:
                self.stats.register_fail += 1

        elif cmd == "update_embedded_info":
            self._name = params.get("name", self._name)
            self._group = params.get("group", self._group)
            sender = params.get("sender", -1)
            self.seq += 1
            ack_params = {
                "sender": sender,
                "group": self._group,
                "name": self._name,
                "msg": "ok",
            }
            await self.send_line(
                make_msg("embedded", "update_info_ack", self.seq, ack_params)
            )

        elif cmd == "push_resources_to_download":
            # 压测不真正下载 HTTP，只统计收到推送
            pass

        elif cmd == "push_schedule_playlist":
            pass

        elif cmd == "request_screenshot":
            client_id = params.get("device_id", -1)
            if client_id is None:
                client_id = -1
            self.seq += 1
            ack_params = {
                "device_id": int(client_id),
                "path": f"/screenshots/STRESS-{self.index:04d}.png",
            }
            await self.send_line(
                make_msg("embedded", "screenshot_data", self.seq, ack_params)
            )

        elif cmd == "ota_update":
            device_uid = params.get("device_uid", "")
            path = params.get("path", "")
            self.seq += 1
            ack_params = {
                "code": 0,
                "result": 1,
                "msg": "stress test ack",
                "path": path,
                "device_uid": device_uid,
            }
            await self.send_line(
                make_msg("embedded", "ota_update_ack", self.seq, ack_params)
            )

    async def read_loop(self, stop_event: asyncio.Event) -> None:
        """@brief: 持续读取服务端消息并按行解析"""
        assert self._reader is not None
        buf = b""
        while not stop_event.is_set():
            try:
                chunk = await asyncio.wait_for(self._reader.read(4096), timeout=1.0)
            except asyncio.TimeoutError:
                continue
            if not chunk:
                break
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.strip()
                if not line:
                    continue
                try:
                    msg = json.loads(line.decode("utf-8"))
                    await self.handle_server_cmd(msg)
                except json.JSONDecodeError:
                    pass

    async def heartbeat_loop(self, stop_event: asyncio.Event) -> None:
        """@brief: 注册成功后每 5 秒发送一次心跳"""
        while not stop_event.is_set():
            await asyncio.sleep(HEARTBEAT_INTERVAL_SEC)
            if self.registered:
                try:
                    await self.send_heartbeat()
                except (ConnectionError, OSError, asyncio.IncompleteReadError):
                    break

    async def run(self, stop_event: asyncio.Event, connect_timeout: float) -> bool:
        """
        @brief: 连接服务器并保持到 stop_event 被设置
        @param: stop_event，档位结束时由外部置位
        @param: connect_timeout，TCP 连接超时秒数
        @retval: True 表示注册成功且正常结束；False 表示连接或注册失败
        """
        self.stats.connect_attempts += 1
        try:
            self._reader, self._writer = await asyncio.wait_for(
                asyncio.open_connection(self.host, self.port),
                timeout=connect_timeout,
            )
        except (OSError, asyncio.TimeoutError):
            return False

        self.stats.connect_ok += 1
        read_task = asyncio.create_task(self.read_loop(stop_event))
        hb_task = asyncio.create_task(self.heartbeat_loop(stop_event))
        self._tasks = [read_task, hb_task]

        await self.send_register()

        # 等待注册完成，最多 10 秒
        for _ in range(100):
            if self.registered or stop_event.is_set():
                break
            await asyncio.sleep(0.1)

        if not self.registered:
            self.stats.register_fail += 1
            stop_event.set()

        try:
            await read_task
        except Exception:
            if not stop_event.is_set():
                self.stats.unexpected_disconnects += 1

        for t in self._tasks:
            if not t.done():
                t.cancel()
        await asyncio.gather(*self._tasks, return_exceptions=True)

        if self._writer:
            try:
                self._writer.close()
                await self._writer.wait_closed()
            except Exception:
                pass
        return self.registered


async def run_stage(
    host: str,
    port: int,
    stage_name: str,
    count: int,
    duration_sec: int,
    stagger_sec: float,
    connect_timeout: float,
    verbose: bool,
) -> StageResult:
    """
    @brief: 运行单档压测：启动 count 个模拟设备，浸泡 duration_sec 秒后回收
    @param: host/port，服务端地址
    @param: stage_name，档位名称 L1/L2/L3
    @param: count，模拟设备数量
    @param: duration_sec，本档运行时长（秒）
    @param: stagger_sec，每台设备启动间隔，避免瞬时连接风暴
    @param: connect_timeout，单设备 TCP 连接超时
    @param: verbose，是否打印每台设备注册日志
    @retval: StageResult 本档统计与是否通过
    """
    stats = Stats()
    stop_event = asyncio.Event()
    simulators: List[EmbeddedSimulator] = []
    tasks: List[asyncio.Task] = []

    started = time.time()
    started_at = datetime.now().isoformat(timespec="seconds")

    print(f"\n{'=' * 60}")
    print(f"  档位 {stage_name}: {count} 设备 × {duration_sec}s")
    print(f"  目标: {host}:{port}  启动间隔: {stagger_sec}s")
    print(f"{'=' * 60}")

    for i in range(count):
        if STOP_REQUESTED:
            break
        sim = EmbeddedSimulator(i, host, port, stats, verbose)
        simulators.append(sim)
        tasks.append(asyncio.create_task(sim.run(stop_event, connect_timeout)))
        if stagger_sec > 0 and i + 1 < count:
            await asyncio.sleep(stagger_sec)

    # 进度条式日志，每 10 秒打印一次
    elapsed = 0
    while elapsed < duration_sec and not STOP_REQUESTED:
        await asyncio.sleep(min(10, duration_sec - elapsed))
        elapsed = int(time.time() - started)
        reg = stats.register_ok
        print(
            f"  [{stage_name}] {elapsed}s/{duration_sec}s  "
            f"连接={stats.connect_ok}/{count} 注册={reg}/{count}  "
            f"心跳={stats.heartbeats_sent} 断连={stats.unexpected_disconnects}"
        )

    stop_event.set()
    results = await asyncio.gather(*tasks, return_exceptions=True)
    registered_at_end = sum(1 for r in results if r is True)

    ended_at = datetime.now().isoformat(timespec="seconds")
    total_elapsed = time.time() - started

    passed, reason = evaluate_stage(count, duration_sec, stats, registered_at_end)

    result = StageResult(
        name=stage_name,
        target_count=count,
        duration_sec=duration_sec,
        started_at=started_at,
        ended_at=ended_at,
        elapsed_sec=round(total_elapsed, 2),
        stats=stats.snapshot(),
        registered_at_end=registered_at_end,
        passed=passed,
        fail_reason=reason,
    )
    print_stage_summary(result)
    return result


def evaluate_stage(
    target_count: int,
    duration_sec: int,
    stats: Stats,
    registered_at_end: int,
) -> tuple[bool, str]:
    """
    @brief: 判定单档是否通过
    @param: target_count，目标设备数
    @param: duration_sec，本档时长（用于估算心跳下限）
    @param: stats，本档计数
    @param: registered_at_end，结束时仍注册成功的设备数
    @retval: (passed, fail_reason)
    """
    min_register = max(1, int(target_count * 0.99))
    if stats.register_ok < min_register:
        return False, f"注册成功 {stats.register_ok}/{target_count} < 99%"

    if registered_at_end < min_register:
        return False, f"结束时在线注册 {registered_at_end}/{target_count} < 99%"

    if stats.unexpected_disconnects > 0:
        return False, f"异常断连 {stats.unexpected_disconnects} 次"

    # 心跳应约为 count * (duration/5) * 0.8（允许启动阶段偏少）
    expected_hb = target_count * (duration_sec / HEARTBEAT_INTERVAL_SEC) * 0.5
    if stats.heartbeats_sent < expected_hb:
        return False, (
            f"心跳过少 {stats.heartbeats_sent}，期望至少约 {int(expected_hb)}"
        )

    return True, ""


def print_stage_summary(result: StageResult) -> None:
    """@brief: 在终端打印单档结果摘要"""
    status = "PASS ✅" if result.passed else "FAIL ❌"
    print(f"\n--- {result.name} 结果: {status} ---")
    print(f"  目标连接数:     {result.target_count}")
    print(f"  注册成功:       {result.stats['register_ok']}")
    print(f"  结束时仍在线:   {result.registered_at_end}")
    print(f"  心跳发送:       {result.stats['heartbeats_sent']}")
    print(f"  异常断连:       {result.stats['unexpected_disconnects']}")
    cmds = result.stats.get("cmd_received") or {}
    if cmds:
        print(f"  收到指令:       {cmds}")
    if result.fail_reason:
        print(f"  失败原因:       {result.fail_reason}")
    print(f"  耗时:           {result.elapsed_sec}s")


def save_report(
    report_path: str,
    host: str,
    port: int,
    mode: str,
    results: List[StageResult],
) -> None:
    """
    @brief: 将压测结果写入 JSON 报告文件
    @param: report_path，输出路径
    @param: host/port/mode，测试配置
    @param: results，各档 StageResult 列表
    """
    os.makedirs(os.path.dirname(report_path) or ".", exist_ok=True)
    payload = {
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "host": host,
        "port": port,
        "mode": mode,
        "all_passed": all(r.passed for r in results),
        "stages": [asdict(r) for r in results],
    }
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False, indent=2)
    print(f"\n📄 报告已保存: {report_path}")


def install_signal_handlers() -> None:
    """@brief: 注册 SIGINT/SIGTERM，支持 Ctrl+C 优雅停止"""

    def _handler(signum, frame):
        global STOP_REQUESTED
        STOP_REQUESTED = True
        print("\n⏳ 收到停止信号，当前档位结束后退出…")

    signal.signal(signal.SIGINT, _handler)
    signal.signal(signal.SIGTERM, _handler)


async def async_main(args: argparse.Namespace) -> int:
    """
    @brief: 异步主入口，根据 mode 执行 soak 或 ramp
    @retval: 进程退出码，0 表示全部档位通过
    """
    install_signal_handlers()

    if args.mode == "soak":
        stages = [{
            "name": "soak",
            "count": args.count,
            "duration_sec": args.duration,
        }]
    else:
        stages = parse_stages(args.stages) if args.stages else DEFAULT_STAGES

    results: List[StageResult] = []
    for i, stage in enumerate(stages):
        if STOP_REQUESTED:
            break
        result = await run_stage(
            host=args.host,
            port=args.port,
            stage_name=stage["name"],
            count=stage["count"],
            duration_sec=stage["duration_sec"],
            stagger_sec=args.stagger,
            connect_timeout=args.connect_timeout,
            verbose=args.verbose,
        )
        results.append(result)
        if not result.passed:
            print(f"\n❌ 档位 {stage['name']} 未通过，停止后续分档。")
            break
        if args.mode == "ramp" and i + 1 < len(stages) and not STOP_REQUESTED:
            print(f"\n⏸  档位间冷却 {STAGE_COOLDOWN_SEC}s …")
            await asyncio.sleep(STAGE_COOLDOWN_SEC)

    report_path = args.report
    if not report_path:
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        report_dir = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "reports"
        )
        report_path = os.path.join(report_dir, f"stress_{ts}.json")

    save_report(report_path, args.host, args.port, args.mode, results)

    all_passed = bool(results) and all(r.passed for r in results)
    if all_passed:
        print("\n🎉 全部档位通过。")
        return 0
    print("\n⚠️  存在未通过档位，请查看报告。")
    return 1


def build_parser() -> argparse.ArgumentParser:
    """@brief: 构建命令行参数解析器"""
    p = argparse.ArgumentParser(
        description="screen-fleet-pilot 嵌入式设备分档压力测试（asyncio）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python stress_device.py --host 127.0.0.1 --mode soak --count 200 --duration 1800
  python stress_device.py --host 8.136.113.168 --mode ramp
  python stress_device.py --mode ramp --stages "20:300,50:900,200:1800"
        """,
    )
    p.add_argument("--host", default="127.0.0.1", help="服务端 IP")
    p.add_argument("--port", type=int, default=8000, help="服务端端口")
    p.add_argument(
        "--mode",
        choices=("soak", "ramp"),
        default="ramp",
        help="soak=单档浸泡, ramp=自动分档 L1→L2→L3",
    )
    p.add_argument("--count", type=int, default=200, help="soak 模式设备数量")
    p.add_argument(
        "--duration",
        type=int,
        default=1800,
        help="soak 模式时长（秒），默认 30 分钟",
    )
    p.add_argument(
        "--stages",
        default="",
        help='ramp 自定义分档，如 "20:300,50:900,200:1800"',
    )
    p.add_argument(
        "--stagger",
        type=float,
        default=0.05,
        help="每台设备启动间隔（秒），默认 0.05",
    )
    p.add_argument(
        "--connect-timeout",
        type=float,
        default=10.0,
        help="单设备 TCP 连接超时（秒）",
    )
    p.add_argument(
        "--report",
        default="",
        help="报告 JSON 路径，默认 scripts/reports/stress_时间戳.json",
    )
    p.add_argument("-v", "--verbose", action="store_true", help="打印每台设备注册日志")
    return p


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    try:
        exit_code = asyncio.run(async_main(args))
    except KeyboardInterrupt:
        exit_code = 130
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
