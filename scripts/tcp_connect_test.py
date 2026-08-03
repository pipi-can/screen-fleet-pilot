#!/usr/bin/env python3
"""
简单 TCP 连接/断开测试

用法：
  python3 tcp_connect_test.py 127.0.0.1 8000 10
  python3 tcp_connect_test.py 192.168.56.1 8000 20
"""

import socket
import sys
import time


def main():
    if len(sys.argv) < 4:
        print("用法: python3 tcp_connect_test.py <host> <port> <count>")
        print("示例: python3 tcp_connect_test.py 127.0.0.1 8000 10")
        sys.exit(1)

    host = sys.argv[1]
    port = int(sys.argv[2])
    count = int(sys.argv[3])

    socks = []

    # 连接
    print(f"连接 {host}:{port}，共 {count} 个客户端...")
    for i in range(count):
        try:
            s = socket.create_connection((host, port), timeout=5)
            socks.append(s)
            print(f"[{i + 1}] 连接成功")
        except OSError as e:
            print(f"[{i + 1}] 连接失败: {e}")

    print(f"\n已连接: {len(socks)}/{count}")
    print("5 秒后断开...")
    time.sleep(5)

    # 断开
    for i, s in enumerate(socks):
        s.close()
        print(f"[{i + 1}] 已断开")

    print("完成")


if __name__ == "__main__":
    main()
