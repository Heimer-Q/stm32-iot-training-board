"""抓串口日志（板子联调用）。

和"串口调试助手"的区别：这个可以**先打开串口、再复位板子**，所以能抓到开机第一条打印。

用法：
    python Tools/serial_capture.py --port COM6 --seconds 8
    python Tools/serial_capture.py --port COM6 --seconds 8 --reset        # 先复位再抓（走 OpenOCD）
    python Tools/serial_capture.py --port COM6 --seconds 8 -o log.txt
    python Tools/serial_capture.py --port COM6 --seconds 8 --send "AT"    # 抓到一半发点数据

参数默认值按培训板：115200-8-N-1。
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError:
    sys.exit("缺 pyserial：pip install pyserial")

OPENOCD = Path(r"D:\OpenOCD\xpack-openocd-0.12.0-7\bin\openocd.exe")
OPENOCD_SCRIPTS = Path(r"D:\OpenOCD\xpack-openocd-0.12.0-7\openocd\scripts")


def target_reset():
    if not OPENOCD.exists():
        print("[warn] 找不到 openocd，跳过复位", file=sys.stderr)
        return
    cmd = [
        str(OPENOCD), "-s", str(OPENOCD_SCRIPTS),
        "-f", "interface/stlink.cfg", "-f", "target/stm32f1x.cfg",
        "-c", "init", "-c", "reset run", "-c", "shutdown",
    ]
    subprocess.run(cmd, capture_output=True, text=True)


def main():
    ap = argparse.ArgumentParser(description="抓串口日志（可先复位再抓）")
    ap.add_argument("--port", default="COM6")
    ap.add_argument("--baudrate", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=8.0)
    ap.add_argument("--reset", action="store_true", help="先复位目标再抓（抓完整启动日志）")
    ap.add_argument("--send", help="抓到 1 秒时发一串文本过去")
    ap.add_argument("--timestamp", action="store_true", help="每行前面加相对时间（秒）")
    ap.add_argument("-o", "--output", help="同时写入这个文件")
    args = ap.parse_args()

    port = serial.Serial(args.port, args.baudrate, timeout=0.5)
    print(f"[ok] {args.port} 已打开 @{args.baudrate}"
          f"{'，正在复位目标…' if args.reset else ''}")

    # 指定 -o 时边抓边落盘（flush），这样后台跑的时候可以随时看文件
    out_file = None
    if args.output:
        out_file = open(args.output, "w", encoding="utf-8", buffering=1)

    if args.reset:
        time.sleep(0.2)          # 先确保串口已经在读
        target_reset()

    buf = bytearray()
    start = time.time()
    send_at = start + 1.0
    sent = False
    pending = ""

    def emit(text):
        """写屏幕 + 写文件（--timestamp 时逐行加相对时间）"""
        nonlocal pending
        if args.timestamp:
            pending += text
            while "\n" in pending:
                line, pending = pending.split("\n", 1)
                stamped = f"[{time.time() - start:6.2f}] {line.rstrip()}\n"
                sys.stdout.write(stamped)
                if out_file:
                    out_file.write(stamped)
        else:
            sys.stdout.write(text)
            if out_file:
                out_file.write(text)
        sys.stdout.flush()

    while time.time() - start < args.seconds:
        chunk = port.read(512)
        if chunk:
            buf += chunk
            emit(chunk.decode("utf-8", "replace"))
        if args.send and not sent and time.time() >= send_at:
            port.write(args.send.encode("utf-8"))
            port.flush()
            print(f"\n[tx ] {args.send}")
            sent = True
    port.close()

    print(f"\n[ok] 共收到 {len(buf)} 字节")
    if out_file:
        out_file.close()
        print(f"[ok] 已写入 {args.output}")


if __name__ == "__main__":
    main()
