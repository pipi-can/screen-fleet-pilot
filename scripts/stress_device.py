#!/usr/bin/env python3
"""
screen-fleet-pilot 嵌入式设备压力测试脚本（asyncio）

与 device-agent/client.c 行为对齐：
  - register → register_ack(code=0) → 5s heartbeat（含 device_id、cpu_temp 字符串）
  - push_resources_to_download / push_schedule_playlist：后台模拟 HTTP 下载，**无 TCP 回包**
  - request_screenshot：延迟后回 screenshot_data（path，非 base64）
  - ota_update：延迟后回 ota_update_ack（含 path/local_path/device_uid）
  - update_embedded_info → update_info_ack
  - 断线自动重连（与真机一致）

可选 --with-client：模拟 Qt 管理端注册、fetch_devices、批量推送/截屏，压测服务端转发链路。

用法：
  python stress_device.py --host 127.0.0.1 --mode soak --count 200 --duration 1800
  python stress_device.py --host 127.0.0.1 --mode ramp --with-client
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
from typing import Any, Dict, List, Optional, Set

DEFAULT_STAGES = [
    {"name": "L1", "count": 20, "duration_sec": 300},
    {"name": "L2", "count": 50, "duration_sec": 900},
    {"name": "L3", "count": 200, "duration_sec": 1800},
]

STAGE_COOLDOWN_SEC = 30
HEARTBEAT_INTERVAL_SEC = 5.0
CLIENT_HEARTBEAT_INTERVAL_SEC = 5.0
DEVICE_VERSION = "1.0.5"
CLIENT_VERSION = "1.0.5"

DEFAULT_PUSH_PATHS = [
    "/uploads/stress_clip_01.mp4",
    "/uploads/stress_image_01.jpg",
    "/uploads/stress_image_02.png",
]

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

STOP_REQUESTED = False


@dataclass
class WorkloadConfig:
    """@brief: 模拟真机侧资源消耗与重连参数"""

    download_ms_per_file: float = 200.0
    screenshot_ms: float = 800.0
    ota_download_ms: float = 3000.0
    reconnect: bool = True
    reconnect_min_sec: float = 2.0
    reconnect_max_sec: float = 8.0
    with_client: bool = False
    fetch_interval_sec: float = 30.0
    push_interval_sec: float = 60.0
    screenshot_interval_sec: float = 120.0
    push_paths: List[str] = field(default_factory=lambda: list(DEFAULT_PUSH_PATHS))
    max_uids_per_push: int = 0  # 0 = 全部在线 STRESS 设备


def make_msg(
    source: str,
    cmd: str,
    seq: int,
    params: Optional[Dict[str, Any]] = None,
    device_id: Optional[int] = None,
) -> str:
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


def split_lines(buf: bytearray) -> List[str]:
    lines: List[str] = []
    while True:
        idx = buf.find(b"\n")
        if idx < 0:
            break
        raw = buf[:idx].decode("utf-8", errors="replace").strip()
        del buf[: idx + 1]
        if raw:
            lines.append(raw)
    return lines


@dataclass
class Stats:
    connect_attempts: int = 0
    connect_ok: int = 0
    register_ok: int = 0
    register_fail: int = 0
    heartbeats_sent: int = 0
    reconnects: int = 0
    unexpected_disconnects: int = 0
    push_resources_received: int = 0
    push_schedule_received: int = 0
    download_jobs_started: int = 0
    download_jobs_finished: int = 0
    screenshots_sent: int = 0
    ota_acks_sent: int = 0
    update_info_acks_sent: int = 0
    cmd_received: Dict[str, int] = field(default_factory=dict)
    client_fetch_devices: int = 0
    client_pushes_sent: int = 0
    client_screenshots_sent: int = 0
    client_heartbeats_sent: int = 0

    def inc_cmd(self, cmd: str) -> None:
        self.cmd_received[cmd] = self.cmd_received.get(cmd, 0) + 1

    def snapshot(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class StageResult:
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
    stages: List[Dict[str, Any]] = []
    for i, part in enumerate(p.strip() for p in text.split(",") if p.strip()):
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
    @brief: 单 embedded 连接，行为对齐 device-agent/client.c
    """

    def __init__(
        self,
        index: int,
        host: str,
        port: int,
        stats: Stats,
        workload: WorkloadConfig,
        verbose: bool,
    ) -> None:
        self.index = index
        self.host = host
        self.port = port
        self.stats = stats
        self.workload = workload
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
        self._cached_files: Set[str] = set()
        self._bg_tasks: Set[asyncio.Task] = set()

    async def send_line(self, line: str) -> None:
        if not self._writer:
            return
        self._writer.write(line.encode("utf-8"))
        await self._writer.drain()

    def _track_bg(self, task: asyncio.Task) -> None:
        self._bg_tasks.add(task)
        task.add_done_callback(self._bg_tasks.discard)

    async def send_register(self) -> None:
        self.seq += 1
        params = {
            "name": self.profile["name"],
            "group": self.profile["group"],
            "version": DEVICE_VERSION,
            "device_uid": self.profile["device_uid"],
        }
        await self.send_line(make_msg("embedded", "register", self.seq, params))

    async def send_heartbeat(self) -> None:
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

    async def _simulate_batch_download(
        self,
        urls: List[str],
        *,
        skip_cached: bool,
    ) -> None:
        """
        @brief: 模拟 client.c batch_download_thread，真机无 TCP 回包
        """
        self.stats.download_jobs_started += 1
        delay_sec = self.workload.download_ms_per_file / 1000.0
        for url in urls:
            if not url:
                continue
            name = url.rsplit("/", 1)[-1] or "download.bin"
            local_key = f"{self.profile['device_uid']}:{name}"
            if skip_cached and local_key in self._cached_files:
                continue
            await asyncio.sleep(delay_sec)
            self._cached_files.add(local_key)
        self.stats.download_jobs_finished += 1

    async def _simulate_screenshot(self, client_id: int) -> None:
        await asyncio.sleep(self.workload.screenshot_ms / 1000.0)
        if not self._writer or not self.registered:
            return
        ts = int(time.time())
        path = f"/screenshots/{self.profile['device_uid']}_{ts}.png"
        self.seq += 1
        await self.send_line(
            make_msg(
                "embedded",
                "screenshot_data",
                self.seq,
                {"device_id": int(client_id), "path": path},
                self.device_id,
            )
        )
        self.stats.screenshots_sent += 1

    async def _simulate_ota(
        self,
        server_path: str,
        client_uid: str,
    ) -> None:
        await asyncio.sleep(self.workload.ota_download_ms / 1000.0)
        if not self._writer or not self.registered:
            return
        name = server_path.rsplit("/", 1)[-1] or "firmware.tgz"
        local_path = f"/var/screen-fleet/firmware/{name}"
        self.seq += 1
        ack_params = {
            "code": 0,
            "result": 1,
            "msg": "ok",
            "path": server_path,
            "local_path": local_path,
            "device_uid": client_uid,
        }
        await self.send_line(
            make_msg("embedded", "ota_update_ack", self.seq, ack_params, self.device_id)
        )
        self.stats.ota_acks_sent += 1

    async def handle_server_cmd(self, msg: Dict[str, Any]) -> None:
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
                    print(
                        f"[#{self.index:04d}] register ok "
                        f"device_id={self.device_id}"
                    )
            else:
                self.stats.register_fail += 1

        elif cmd == "update_embedded_info":
            self._name = params.get("name", self._name)
            self._group = params.get("group", self._group)
            sender = params.get("sender", -1)
            self.seq += 1
            await self.send_line(
                make_msg(
                    "embedded",
                    "update_info_ack",
                    self.seq,
                    {
                        "sender": sender,
                        "group": self._group,
                        "name": self._name,
                        "msg": "ok",
                    },
                    self.device_id,
                )
            )
            self.stats.update_info_acks_sent += 1

        elif cmd == "push_resources_to_download":
            # 真机：pthread 下载，无 TCP ack
            self.stats.push_resources_received += 1
            paths = params.get("paths") or []
            if isinstance(paths, list) and paths:
                task = asyncio.create_task(
                    self._simulate_batch_download(
                        [str(p) for p in paths],
                        skip_cached=False,
                    )
                )
                self._track_bg(task)

        elif cmd == "push_schedule_playlist":
            # 真机：写 schedule.json + 仅下载缺失文件，无 TCP ack
            self.stats.push_schedule_received += 1
            paths = params.get("paths") or []
            if isinstance(paths, list) and paths:
                task = asyncio.create_task(
                    self._simulate_batch_download(
                        [str(p) for p in paths],
                        skip_cached=True,
                    )
                )
                self._track_bg(task)

        elif cmd == "request_screenshot":
            client_id = params.get("device_id", -1)
            if client_id is None:
                client_id = -1
            task = asyncio.create_task(self._simulate_screenshot(int(client_id)))
            self._track_bg(task)

        elif cmd == "ota_update":
            server_path = str(params.get("path", ""))
            client_uid = str(params.get("device_uid", ""))
            if server_path:
                task = asyncio.create_task(
                    self._simulate_ota(server_path, client_uid)
                )
                self._track_bg(task)

    async def read_loop(self, stop_event: asyncio.Event) -> None:
        assert self._reader is not None
        buf = bytearray()
        while not stop_event.is_set():
            try:
                chunk = await asyncio.wait_for(self._reader.read(4096), timeout=1.0)
            except asyncio.TimeoutError:
                continue
            if not chunk:
                break
            buf += chunk
            for line in split_lines(buf):
                try:
                    msg = json.loads(line)
                    await self.handle_server_cmd(msg)
                except json.JSONDecodeError:
                    pass

    async def heartbeat_loop(self, stop_event: asyncio.Event) -> None:
        while not stop_event.is_set():
            await asyncio.sleep(HEARTBEAT_INTERVAL_SEC)
            if self.registered:
                try:
                    await self.send_heartbeat()
                except (ConnectionError, OSError, asyncio.IncompleteReadError):
                    break

    async def _close_connection(self) -> None:
        for t in self._tasks:
            if not t.done():
                t.cancel()
        if self._tasks:
            await asyncio.gather(*self._tasks, return_exceptions=True)
        self._tasks.clear()

        for t in list(self._bg_tasks):
            if not t.done():
                t.cancel()
        if self._bg_tasks:
            await asyncio.gather(*self._bg_tasks, return_exceptions=True)
        self._bg_tasks.clear()

        if self._writer:
            try:
                self._writer.close()
                await self._writer.wait_closed()
            except Exception:
                pass
        self._reader = None
        self._writer = None

    async def _session_once(
        self,
        stop_event: asyncio.Event,
        connect_timeout: float,
    ) -> bool:
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
        for _ in range(100):
            if self.registered or stop_event.is_set():
                break
            await asyncio.sleep(0.1)

        if not self.registered:
            self.stats.register_fail += 1
            await self._close_connection()
            return False

        try:
            await read_task
        except Exception:
            if not stop_event.is_set():
                self.stats.unexpected_disconnects += 1

        await self._close_connection()
        return self.registered

    async def run(self, stop_event: asyncio.Event, connect_timeout: float) -> bool:
        ever_registered = False
        while not stop_event.is_set():
            ok = await self._session_once(stop_event, connect_timeout)
            if ok:
                ever_registered = True
            if stop_event.is_set():
                break
            if not self.workload.reconnect:
                break
            self.registered = False
            self.device_id = None
            self.stats.reconnects += 1
            delay = random.uniform(
                self.workload.reconnect_min_sec,
                self.workload.reconnect_max_sec,
            )
            await asyncio.sleep(delay)
        return ever_registered


class ClientLoadGenerator:
    """
    @brief: 模拟 Qt NetworkManager：注册、fetch_devices、批量推送与截屏
    """

    def __init__(
        self,
        host: str,
        port: int,
        stats: Stats,
        workload: WorkloadConfig,
    ) -> None:
        self.host = host
        self.port = port
        self.stats = stats
        self.workload = workload
        self.client_uid = "STRESS-CLIENT-ADMIN"
        self.seq = 0
        self.device_id: Optional[int] = None
        self.registered = False
        self.online_uids: List[str] = []
        self._reader: Optional[asyncio.StreamReader] = None
        self._writer: Optional[asyncio.StreamWriter] = None

    async def send_line(self, line: str) -> None:
        if not self._writer:
            return
        self._writer.write(line.encode("utf-8"))
        await self._writer.drain()

    async def send_register(self) -> None:
        self.seq += 1
        params = {
            "name": "压测管理端",
            "group": "stress",
            "version": CLIENT_VERSION,
            "device_uid": self.client_uid,
        }
        await self.send_line(make_msg("client", "register", self.seq, params))

    async def send_heartbeat(self) -> None:
        if not self.registered or self.device_id is None:
            return
        self.seq += 1
        await self.send_line(
            make_msg("client", "heartbeat", self.seq, {}, self.device_id)
        )
        self.stats.client_heartbeats_sent += 1

    async def send_fetch_devices(self) -> None:
        if not self.registered:
            return
        self.seq += 1
        await self.send_line(
            make_msg("client", "fetch_devices", self.seq, {})
        )

    async def send_push_content(self, uids: List[str]) -> None:
        if not self.registered or not uids:
            return
        self.seq += 1
        params = {
            "device_uids": uids,
            "paths": self.workload.push_paths,
        }
        await self.send_line(
            make_msg("client", "request_push_content_to_embedded", self.seq, params)
        )
        self.stats.client_pushes_sent += 1

    async def send_screenshot_request(self, uid: str) -> None:
        if not self.registered or not uid:
            return
        self.seq += 1
        params = {"device_uid": uid}
        await self.send_line(
            make_msg("client", "request_screenshot", self.seq, params)
        )
        self.stats.client_screenshots_sent += 1

    def _update_online_uids(self, msg: Dict[str, Any]) -> None:
        params = msg.get("params") or {}
        devices = params.get("devices") or []
        uids: List[str] = []
        for dev in devices:
            if not isinstance(dev, dict):
                continue
            uid = dev.get("device_uid", "")
            if not uid or not str(uid).startswith("STRESS-"):
                continue
            if dev.get("online") is True:
                uids.append(str(uid))
        self.online_uids = uids

    async def read_loop(self, stop_event: asyncio.Event) -> None:
        assert self._reader is not None
        buf = bytearray()
        while not stop_event.is_set():
            try:
                chunk = await asyncio.wait_for(self._reader.read(4096), timeout=1.0)
            except asyncio.TimeoutError:
                continue
            if not chunk:
                break
            buf += chunk
            for line in split_lines(buf):
                try:
                    msg = json.loads(line)
                except json.JSONDecodeError:
                    continue
                cmd = msg.get("cmd", "")
                params = msg.get("params") or {}
                if cmd == "register_ack" and params.get("code") == 0:
                    self.device_id = params.get("device_id")
                    self.registered = True
                elif cmd == "fetch_devices_ack":
                    self.stats.client_fetch_devices += 1
                    self._update_online_uids(msg)
                self.stats.inc_cmd(f"client_rx:{cmd}")

    async def periodic_loop(self, stop_event: asyncio.Event) -> None:
        last_fetch = 0.0
        last_push = 0.0
        last_shot = 0.0
        last_hb = 0.0
        shot_idx = 0

        while not stop_event.is_set():
            now = time.time()
            if self.registered:
                if now - last_hb >= CLIENT_HEARTBEAT_INTERVAL_SEC:
                    await self.send_heartbeat()
                    last_hb = now
                if now - last_fetch >= self.workload.fetch_interval_sec:
                    await self.send_fetch_devices()
                    last_fetch = now
                if (
                    now - last_push >= self.workload.push_interval_sec
                    and self.online_uids
                ):
                    uids = self.online_uids
                    cap = self.workload.max_uids_per_push
                    if cap > 0:
                        uids = uids[:cap]
                    await self.send_push_content(uids)
                    last_push = now
                if now - last_shot >= self.workload.screenshot_interval_sec:
                    if self.online_uids:
                        uid = self.online_uids[shot_idx % len(self.online_uids)]
                        shot_idx += 1
                        await self.send_screenshot_request(uid)
                    last_shot = now
            await asyncio.sleep(0.5)

    async def run(self, stop_event: asyncio.Event, connect_timeout: float) -> None:
        try:
            self._reader, self._writer = await asyncio.wait_for(
                asyncio.open_connection(self.host, self.port),
                timeout=connect_timeout,
            )
        except (OSError, asyncio.TimeoutError):
            print("[client-load] 连接失败")
            return

        read_task = asyncio.create_task(self.read_loop(stop_event))
        work_task = asyncio.create_task(self.periodic_loop(stop_event))
        await self.send_register()

        for _ in range(50):
            if self.registered or stop_event.is_set():
                break
            await asyncio.sleep(0.1)

        if self.registered:
            print(
                f"[client-load] 管理端注册成功 device_id={self.device_id}，"
                f"将周期性 fetch/push/screenshot"
            )
            await self.send_fetch_devices()
        else:
            print("[client-load] 管理端注册失败")

        await asyncio.gather(read_task, work_task, return_exceptions=True)

        if self._writer:
            try:
                self._writer.close()
                await self._writer.wait_closed()
            except Exception:
                pass


async def run_stage(
    host: str,
    port: int,
    stage_name: str,
    count: int,
    duration_sec: int,
    stagger_sec: float,
    connect_timeout: float,
    workload: WorkloadConfig,
    verbose: bool,
) -> StageResult:
    stats = Stats()
    stop_event = asyncio.Event()
    simulators: List[EmbeddedSimulator] = []
    tasks: List[asyncio.Task] = []

    started = time.time()
    started_at = datetime.now().isoformat(timespec="seconds")

    print(f"\n{'=' * 60}")
    print(f"  档位 {stage_name}: {count} 设备 × {duration_sec}s")
    print(f"  目标: {host}:{port}  启动间隔: {stagger_sec}s")
    if workload.with_client:
        print(
            f"  管理端负载: fetch/{workload.fetch_interval_sec}s "
            f"push/{workload.push_interval_sec}s "
            f"screenshot/{workload.screenshot_interval_sec}s"
        )
    print(f"{'=' * 60}")

    if workload.with_client:
        client = ClientLoadGenerator(host, port, stats, workload)
        tasks.append(
            asyncio.create_task(client.run(stop_event, connect_timeout))
        )

    for i in range(count):
        if STOP_REQUESTED:
            break
        sim = EmbeddedSimulator(i, host, port, stats, workload, verbose)
        simulators.append(sim)
        tasks.append(asyncio.create_task(sim.run(stop_event, connect_timeout)))
        if stagger_sec > 0 and i + 1 < count:
            await asyncio.sleep(stagger_sec)

    elapsed = 0
    while elapsed < duration_sec and not STOP_REQUESTED:
        await asyncio.sleep(min(10, duration_sec - elapsed))
        elapsed = int(time.time() - started)
        reg = stats.register_ok
        print(
            f"  [{stage_name}] {elapsed}s/{duration_sec}s  "
            f"连接={stats.connect_ok} 注册={reg} 心跳={stats.heartbeats_sent} "
            f"push={stats.push_resources_received} 下载={stats.download_jobs_finished} "
            f"截屏={stats.screenshots_sent} 重连={stats.reconnects} "
            f"断连={stats.unexpected_disconnects}"
        )
        if workload.with_client:
            print(
                f"           client: fetch={stats.client_fetch_devices} "
                f"push={stats.client_pushes_sent} "
                f"screenshot={stats.client_screenshots_sent}"
            )

    stop_event.set()
    results = await asyncio.gather(*tasks, return_exceptions=True)
    embedded_results = results[-count:] if workload.with_client else results
    registered_at_end = sum(1 for r in embedded_results if r is True)

    ended_at = datetime.now().isoformat(timespec="seconds")
    total_elapsed = time.time() - started
    passed, reason = evaluate_stage(count, duration_sec, stats, registered_at_end, workload)

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
    workload: WorkloadConfig,
) -> tuple[bool, str]:
    min_register = max(1, int(target_count * 0.99))
    if stats.register_ok < min_register:
        return False, f"注册成功 {stats.register_ok}/{target_count} < 99%"

    if registered_at_end < min_register:
        return False, f"结束时在线注册 {registered_at_end}/{target_count} < 99%"

    if stats.unexpected_disconnects > target_count * 2:
        return False, f"异常断连过多 {stats.unexpected_disconnects}"

    expected_hb = target_count * (duration_sec / HEARTBEAT_INTERVAL_SEC) * 0.5
    if stats.heartbeats_sent < expected_hb:
        return False, (
            f"心跳过少 {stats.heartbeats_sent}，期望至少约 {int(expected_hb)}"
        )

    if workload.with_client and stats.client_fetch_devices < 1:
        return False, "管理端未成功 fetch_devices"

    if (
        workload.with_client
        and stats.client_pushes_sent < 1
        and duration_sec >= int(workload.push_interval_sec) + 30
    ):
        return False, "管理端未发起 push_content"

    return True, ""


def print_stage_summary(result: StageResult) -> None:
    status = "PASS ✅" if result.passed else "FAIL ❌"
    s = result.stats
    print(f"\n--- {result.name} 结果: {status} ---")
    print(f"  目标连接数:       {result.target_count}")
    print(f"  注册成功:         {s['register_ok']}")
    print(f"  结束时仍在线:     {result.registered_at_end}")
    print(f"  心跳发送:         {s['heartbeats_sent']}")
    print(f"  push_resources:   {s['push_resources_received']}")
    print(f"  push_schedule:    {s['push_schedule_received']}")
    print(f"  模拟下载完成:     {s['download_jobs_finished']}")
    print(f"  截屏回传:         {s['screenshots_sent']}")
    print(f"  OTA ack:          {s['ota_acks_sent']}")
    print(f"  重连次数:         {s['reconnects']}")
    print(f"  异常断连:         {s['unexpected_disconnects']}")
    if s.get("client_fetch_devices"):
        print(f"  管理端 fetch:     {s['client_fetch_devices']}")
        print(f"  管理端 push:      {s['client_pushes_sent']}")
        print(f"  管理端截屏请求:   {s['client_screenshots_sent']}")
    cmds = s.get("cmd_received") or {}
    if cmds:
        print(f"  收到指令统计:     {cmds}")
    if result.fail_reason:
        print(f"  失败原因:         {result.fail_reason}")
    print(f"  耗时:             {result.elapsed_sec}s")


def save_report(
    report_path: str,
    host: str,
    port: int,
    mode: str,
    results: List[StageResult],
) -> None:
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
    def _handler(signum, frame):
        global STOP_REQUESTED
        STOP_REQUESTED = True
        print("\n⏳ 收到停止信号，当前档位结束后退出…")

    signal.signal(signal.SIGINT, _handler)
    signal.signal(signal.SIGTERM, _handler)


def build_workload(args: argparse.Namespace) -> WorkloadConfig:
    paths = [p.strip() for p in args.push_paths.split(",") if p.strip()]
    return WorkloadConfig(
        download_ms_per_file=args.download_ms,
        screenshot_ms=args.screenshot_ms,
        ota_download_ms=args.ota_ms,
        reconnect=not args.no_reconnect,
        with_client=args.with_client,
        fetch_interval_sec=args.fetch_interval,
        push_interval_sec=args.push_interval,
        screenshot_interval_sec=args.screenshot_interval,
        push_paths=paths or list(DEFAULT_PUSH_PATHS),
        max_uids_per_push=args.max_uids_per_push,
    )


async def async_main(args: argparse.Namespace) -> int:
    install_signal_handlers()
    workload = build_workload(args)

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
            workload=workload,
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
    p = argparse.ArgumentParser(
        description="screen-fleet-pilot 嵌入式压测（行为对齐 device-agent）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 仅 embedded 心跳 soak
  python stress_device.py --host 127.0.0.1 --mode soak --count 200 --duration 600

  # 完整链路：embedded + 管理端批量 push/截屏
  python stress_device.py --host 127.0.0.1 --mode ramp --with-client

  # 加重下载/截屏耗时，模拟弱网设备
  python stress_device.py --with-client --download-ms 500 --screenshot-ms 2000
        """,
    )
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8000)
    p.add_argument("--mode", choices=("soak", "ramp"), default="ramp")
    p.add_argument("--count", type=int, default=200)
    p.add_argument("--duration", type=int, default=1800)
    p.add_argument("--stages", default="")
    p.add_argument("--stagger", type=float, default=0.05)
    p.add_argument("--connect-timeout", type=float, default=10.0)
    p.add_argument("--report", default="")
    p.add_argument("-v", "--verbose", action="store_true")

    g = p.add_argument_group("真机行为模拟")
    g.add_argument(
        "--download-ms",
        type=float,
        default=200.0,
        help="每个资源 URL 模拟下载耗时（毫秒），对齐 curl 下载",
    )
    g.add_argument(
        "--screenshot-ms",
        type=float,
        default=800.0,
        help="截屏+上传模拟耗时（毫秒）",
    )
    g.add_argument(
        "--ota-ms",
        type=float,
        default=3000.0,
        help="OTA 下载+校验模拟耗时（毫秒）",
    )
    g.add_argument(
        "--no-reconnect",
        action="store_true",
        help="断线不重连（旧版压测行为）",
    )

    c = p.add_argument_group("管理端负载（--with-client）")
    c.add_argument(
        "--with-client",
        action="store_true",
        help="额外模拟 Qt 管理端：fetch_devices + 批量 push + 截屏",
    )
    c.add_argument("--fetch-interval", type=float, default=30.0)
    c.add_argument("--push-interval", type=float, default=60.0)
    c.add_argument("--screenshot-interval", type=float, default=120.0)
    c.add_argument(
        "--push-paths",
        default=",".join(DEFAULT_PUSH_PATHS),
        help="推送资源路径，逗号分隔",
    )
    c.add_argument(
        "--max-uids-per-push",
        type=int,
        default=0,
        help="单次 push 最多设备数，0=全部在线 STRESS 设备",
    )
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
