"""把"取模软件导出的多帧文本"转成 OLED 动图数组（oled_gif.h）。

支持的输入格式（常见取模软件导出的样子）：

    // 'frame_00_delay-0', 200x200px
    0xff, 0xff, 0xff, ...
    // 'frame_01_delay-0', 200x200px
    0xff, 0xff, ...

约定（与多数取模软件的默认设置一致）：
  * 每帧 WxH 像素，**逐行式**存放，每行 W/8 字节，**最高位在最左**；
  * 文本里 0 = 黑色（图案内容）、1 = 白色（背景）。工具会把"内容"提取出来缩放，
    再按本工程 OLED 驱动要求打包（1 = 点亮）。

用法：
    python Tools/make_gif.py 取模文本.txt                  # 缩到 64x64
    python Tools/make_gif.py 取模文本.txt --size 64 --delay 80 --max-frames 20
    python Tools/make_gif.py 取模文本.txt --invert         # 如果你的取模是"1=内容"就加这个

输出：
    Hardware/Inc/oled_gif.h
      #define OLED_GIF_W 64 / OLED_GIF_H 64 / OLED_GIF_COUNT n / OLED_GIF_DELAY_MS 80
      static const uint8_t OLED_GIF_0[] = {...};     /* 每帧 64x8 = 512 字节 */
      static const uint8_t * const OLED_GIF[] = { OLED_GIF_0, ... };

显示（图像页）：
    OLED_SetCursor(&g_oled, (128 - OLED_GIF_W) / 2, 0);
    OLED_DrawBitmap(&g_oled, OLED_GIF_W, OLED_GIF_H, OLED_GIF[i]);
"""

import argparse
import re
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Hardware" / "Inc" / "oled_gif.h"
SCREEN_W, SCREEN_H = 128, 64


def parse_frames(text):
    """按 // 'frame_XX', WxHpx 分帧，返回 [(name, w, h, bytes), ...]"""
    parts = re.split(r"//\s*'([^']+)'\s*,\s*(\d+)x(\d+)\s*px", text)
    frames = []
    for i in range(1, len(parts), 4):
        name, w, h, body = parts[i], int(parts[i + 1]), int(parts[i + 2]), parts[i + 3]
        data = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body))
        expect = w * h // 8
        if len(data) != expect:
            print(f"[warn] {name}: {len(data)} 字节，按 {w}x{h} 应为 {expect} 字节，已截断/补零",
                  file=sys.stderr)
            data = (data + bytes(expect))[:expect]
        frames.append((name, w, h, data))
    return frames


def frame_to_image(w, h, data, invert):
    """取模字节 → PIL 灰度图（内容=黑 0，背景=白 255）"""
    stride = w // 8
    img = Image.new("L", (w, h), 255)
    px = img.load()
    for y in range(h):
        for x in range(w):
            bit = (data[y * stride + x // 8] >> (7 - (x % 8))) & 1
            content = (bit == 1) if invert else (bit == 0)
            if content:
                px[x, y] = 0
    return img


def pack(img, size, threshold=128):
    """缩到 size x size，再按驱动要求打包：逐行、每行 size/8 字节、最高位在左、1=点亮"""
    img = img.resize((size, size), Image.LANCZOS)
    px = img.load()
    stride = size // 8
    out = bytearray(stride * size)
    for y in range(size):
        for x in range(size):
            if px[x, y] < threshold:          # 黑 = 内容 = 点亮
                out[y * stride + x // 8] |= 0x80 >> (x % 8)
    return bytes(out)


def order_score(imgs, order):
    """按给定顺序播放时，相邻帧的平均差异（越小 = 越连贯）"""
    total = 0
    n = len(order)
    for i in range(n):
        a = imgs[order[i]]
        b = imgs[order[(i + 1) % n]]
        pa, pb = a.load(), b.load()
        for y in range(0, a.height, 4):
            for x in range(0, a.width, 4):
                if (pa[x, y] < 128) != (pb[x, y] < 128):
                    total += 1
    return total / max(1, n)


def peek(img, step=2):
    """用字符画把一帧打到终端，用来核对取模有没有解析反"""
    px = img.load()
    for y in range(0, img.height, step * 2):
        print("".join("#" if px[x, y] < 128 else "." for x in range(0, img.width, step)))


def main():
    ap = argparse.ArgumentParser(description="取模文本 → OLED 动图数组")
    ap.add_argument("source", help="取模软件导出的 txt")
    ap.add_argument("--size", type=int, default=64, help="缩放到多少像素见方（默认 64，正好塞进 64 高的屏）")
    ap.add_argument("--delay", type=int, default=80, help="每帧显示时长 ms（默认 80）")
    ap.add_argument("--max-frames", type=int, default=0, help="最多取几帧（0 = 全部）")
    ap.add_argument("--step", type=int, default=1, help="抽帧：每隔 step 帧取一帧（帧太多、Flash 紧张时用 2）")
    ap.add_argument("--invert", action="store_true", help="取模里 1=内容、0=背景时加这个")
    ap.add_argument("--keep-order", action="store_true", help="强制按文件顺序，不做顺序判断")
    ap.add_argument("--peek", type=int, default=-1, help="打印第 N 帧的字符画核对")
    args = ap.parse_args()

    src = Path(args.source)
    if not src.exists():
        sys.exit(f"找不到文件：{src}")
    frames = parse_frames(src.read_text(encoding="utf-8", errors="replace"))
    if not frames:
        sys.exit("没解析到任何帧：确认文本里有 // 'frame_XX', WxHpx 这样的行")
    if args.max_frames:
        frames = frames[:args.max_frames]

    # ---- 顺序判断：取模软件导出常常乱序（frame_03 排在 frame_02 前面），用相邻帧差异挑更连贯的那个 ----
    if (len(frames) > 2) and (not args.keep_order):
        imgs_all = [frame_to_image(w, h, d, args.invert) for _, w, h, d in frames]
        idx_file = list(range(len(frames)))
        idx_num = sorted(idx_file, key=lambda i: int(re.search(r"(\d+)", frames[i][0]).group(1)))
        s_file = order_score(imgs_all, idx_file)
        s_num = order_score(imgs_all, idx_num)
        print(f"[ord] 文件顺序差异 {s_file:.1f} / 按帧号排序差异 {s_num:.1f}")
        if s_num < s_file * 0.9:
            frames = [frames[i] for i in idx_num]
            print("[ord] 采用【按帧号排序】（更连贯）")
        else:
            print("[ord] 采用【文件顺序】")

    if 0 <= args.peek < len(frames):
        name, w, h, data = frames[args.peek]
        print(f"[peek] 第 {args.peek} 帧 {name}（# = 图案内容）：")
        peek(frame_to_image(w, h, data, args.invert))

    if args.step > 1:
        frames = frames[::args.step]
        print(f"[step] 抽帧 1/{args.step} → 剩 {len(frames)} 帧")

    size = args.size
    lines = [
        "/* 自动生成，不要手改 —— 由 Tools/make_gif.py 生成 */",
        f"/* 共 {len(frames)} 帧，{size}x{size}，每帧 {size // 8 * size} 字节，"
        f"共 {len(frames) * size // 8 * size} 字节 Flash */",
        "/* 打包：逐行、每行 size/8 字节、最高位在左、1=点亮 */",
        "",
        "#ifndef __OLED_GIF_H",
        "#define __OLED_GIF_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define OLED_GIF_W        {size}U",
        f"#define OLED_GIF_H        {size}U",
        f"#define OLED_GIF_COUNT    {len(frames)}U",
        f"#define OLED_GIF_DELAY_MS {args.delay}U",
        "",
    ]

    for i, (name, w, h, data) in enumerate(frames):
        packed = pack(frame_to_image(w, h, data, args.invert), size)
        lines.append(f"/* [{i}] {name}  ({w}x{h} → {size}x{size}) */")
        lines.append(f"static const uint8_t OLED_GIF_{i}[] = {{")
        for off in range(0, len(packed), 16):
            lines.append("    " + ",".join("0x%02X" % b for b in packed[off:off + 16]) + ",")
        lines.append("};")
        lines.append("")

    lines.append("static const uint8_t * const OLED_GIF[OLED_GIF_COUNT] = {")
    lines.append("    " + ", ".join(f"OLED_GIF_{i}" for i in range(len(frames))) + ",")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* __OLED_GIF_H */")
    lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"[ok] {len(frames)} 帧 → {OUT}")
    print(f"[ok] {size}x{size}，每帧 {size // 8 * size} 字节，合计 {len(frames) * size // 8 * size} 字节")
    print(f"[ok] 屏幕上居中位置：x = {(SCREEN_W - size) // 2}, y = {(SCREEN_H - size) // 2}")


if __name__ == "__main__":
    main()
