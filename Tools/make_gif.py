"""把"取模软件导出的多帧文本"转成 OLED 动图数组（可一次生成多张，按键切换）。

推荐用法：一份配置生成全部动图
    python Tools/make_gif.py --config Tools/gifs.json

Tools/gifs.json 示例：
    {
      "animations": [
        {"name": "anim0", "source": "Tools/gif_src/anim0.txt", "size": [64, 64],  "step": 2, "delay": 70},
        {"name": "anim1", "source": "Tools/gif_src/anim1.txt", "size": [128, 64], "step": 8, "delay": 70}
      ]
    }

只转单张（临时用）：
    python Tools/make_gif.py 取模文本.txt --size 64 --delay 70 --step 2 [--peek 0] [--invert]

支持的输入格式（取模软件导出的样子）：
    // 'frame_00_delay-0', 498x348px
    0xff, 0xff, ...
    // 'frame_01_delay-0', 498x348px
    ...

约定（与多数取模软件默认设置一致）：
  * 每帧 WxH 像素，**逐行式**存放，每行 ceil(W/8) 字节，**最高位在最左**；
  * 文本里 0 = 黑色（图案内容）、1 = 白色（背景）；输出按驱动要求打包（1 = 点亮）。

输出：Hardware/Inc/oled_gif.h
    #define OLED_ANIM_COUNT n
    typedef struct { const uint8_t * const *frames; uint16_t count; uint16_t delay_ms; uint8_t w, h; } OledAnim;
    static const OledAnim OLED_ANIMS[OLED_ANIM_COUNT] = { ... };

显示（图像页）：
    const OledAnim *a = &OLED_ANIMS[i];
    OLED_SetCursor(&g_oled, (128 - a->w) / 2, (64 - a->h) / 2);
    OLED_DrawBitmap(&g_oled, a->w, a->h, a->frames[f]);
"""

import argparse
import json
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
        expect = (w + 7) // 8 * h          # 宽度不是 8 的整数倍时，行跨距向上取整
        if len(data) != expect:
            print(f"[warn] {name}: {len(data)} 字节，按 {w}x{h} 应为 {expect}，已截断/补零",
                  file=sys.stderr)
            data = (data + bytes(expect))[:expect]
        frames.append((name, w, h, data))
    return frames


def frame_to_image(w, h, data, invert):
    """取模字节 → PIL 灰度图（内容=黑 0，背景=白 255）"""
    stride = (w + 7) // 8
    img = Image.new("L", (w, h), 255)
    px = img.load()
    for y in range(h):
        row = data[y * stride:y * stride + stride]
        for x in range(w):
            bit = (row[x // 8] >> (7 - (x % 8))) & 1
            content = (bit == 1) if invert else (bit == 0)
            if content:
                px[x, y] = 0
    return img


def fit_crop(img, out_w, out_h):
    """等比缩放到"铺满"输出尺寸，再居中裁剪（不留黑边）"""
    scale = max(out_w / img.width, out_h / img.height)
    new = img.resize((max(1, int(img.width * scale + 0.5)),
                      max(1, int(img.height * scale + 0.5))), Image.LANCZOS)
    left = (new.width - out_w) // 2
    top = (new.height - out_h) // 2
    return new.crop((left, top, left + out_w, top + out_h))


def pack(img, out_w, out_h, threshold=128):
    """铺满+居中裁剪到 out_w x out_h，再按驱动要求打包（逐行、最高位在左、1=点亮）"""
    img = fit_crop(img, out_w, out_h)
    px = img.load()
    stride = (out_w + 7) // 8
    out = bytearray(stride * out_h)
    for y in range(out_h):
        for x in range(out_w):
            if px[x, y] < threshold:
                out[y * stride + x // 8] |= 0x80 >> (x % 8)
    return bytes(out)


def order_score(imgs, order):
    """按给定顺序播放时相邻帧的平均差异（越小 = 越连贯）"""
    total = 0
    n = len(order)
    for i in range(n):
        a, b = imgs[order[i]], imgs[order[(i + 1) % n]]
        pa, pb = a.load(), b.load()
        for y in range(0, a.height, 8):
            for x in range(0, a.width, 8):
                if (pa[x, y] < 128) != (pb[x, y] < 128):
                    total += 1
    return total / max(1, n)


def fix_order(frames, invert, keep_order):
    """取模导出常常乱序：用相邻帧差异挑更连贯的顺序"""
    if keep_order or len(frames) < 3:
        return frames, "文件顺序(未判断)"
    imgs = [frame_to_image(w, h, d, invert) for _, w, h, d in frames]
    idx_file = list(range(len(frames)))
    idx_num = sorted(idx_file, key=lambda i: int(re.search(r"(\d+)", frames[i][0]).group(1)))
    s_file, s_num = order_score(imgs, idx_file), order_score(imgs, idx_num)
    if s_num < s_file * 0.9:
        return [frames[i] for i in idx_num], f"按帧号排序(差异 {s_file:.0f} → {s_num:.0f})"
    return frames, f"文件顺序(差异 {s_file:.0f} vs {s_num:.0f})"


def peek(img, step=2):
    """字符画核对一帧内容"""
    px = img.load()
    for y in range(0, img.height, step * 2):
        print("".join("#" if px[x, y] < 128 else "." for x in range(0, img.width, step)))


def build_one(anim, index, keep_order=False, peek_at=-1):
    """处理一张动图，返回 (帧字节列表, 宽, 高, 信息串)"""
    src = Path(anim["source"])
    if not src.is_absolute():
        src = ROOT / src
    if not src.exists():
        sys.exit(f"找不到取模文件：{src}")

    size = anim.get("size", [64, 64])
    out_w, out_h = int(size[0]), int(size[1])
    invert = bool(anim.get("invert", False))
    step = max(1, int(anim.get("step", 1)))
    max_frames = int(anim.get("max_frames", 0))

    frames = parse_frames(src.read_text(encoding="utf-8", errors="replace"))
    if not frames:
        sys.exit(f"{src} 里没解析到帧：确认有 // 'frame_XX', WxHpx 这样的行")

    if peek_at is not None and 0 <= peek_at < len(frames):
        print(f"[peek] {anim['name']} 第 {peek_at} 帧（# = 图案内容）：")
        peek(frame_to_image(frames[peek_at][1], frames[peek_at][2], frames[peek_at][3], invert))

    frames, how = fix_order(frames, invert, keep_order)
    if step > 1:
        frames = frames[::step]
    if max_frames:
        frames = frames[:max_frames]

    packed = [pack(frame_to_image(w, h, d, invert), out_w, out_h) for _, w, h, d in frames]
    info = (f"{anim['name']}: {frames[0][1]}x{frames[0][2]} → {out_w}x{out_h}, "
            f"{len(frames)} 帧, step={step}, {how}")
    return packed, out_w, out_h, info


def emit(anims):
    """anims: [(name, packed_frames, w, h, delay_ms), ...]"""
    total = sum(len(f) * ((w + 7) // 8 * h) for _, f, w, h, _ in anims)
    lines = [
        "/* 自动生成，不要手改 —— 由 Tools/make_gif.py 生成 */",
        f"/* 共 {len(anims)} 张动图，合计 {total} 字节 Flash */",
        "/* 打包：逐行、每行 ceil(宽/8) 字节、最高位在左、1=点亮 */",
        "",
        "#ifndef __OLED_GIF_H",
        "#define __OLED_GIF_H",
        "",
        "#include <stdint.h>",
        "",
        "typedef struct {",
        "    const uint8_t * const *frames;   /* 每帧 1024(128x64) 或 512(64x64) 字节 */",
        "    uint16_t count;                  /* 帧数 */",
        "    uint16_t delay_ms;               /* 每帧显示时长 */",
        "    uint8_t  w, h;                   /* 图像尺寸（像素） */",
        "} OledAnim;",
        "",
        f"#define OLED_ANIM_COUNT  {len(anims)}U",
        "",
    ]
    for name, frames, w, h, delay in anims:
        for i, data in enumerate(frames):
            lines.append(f"static const uint8_t {name}_F{i:02d}[] = {{")
            for off in range(0, len(data), 16):
                lines.append("    " + ",".join("0x%02X" % b for b in data[off:off + 16]) + ",")
            lines.append("};")
        lines.append("")
        lines.append(f"static const uint8_t * const {name}_FRAMES[{len(frames)}] = {{")
        lines.append("    " + ", ".join(f"{name}_F{i:02d}" for i in range(len(frames))) + ",")
        lines.append("};")
        lines.append("")

    lines.append("static const OledAnim OLED_ANIMS[OLED_ANIM_COUNT] = {")
    for name, frames, w, h, delay in anims:
        lines.append(f"    {{ {name}_FRAMES, {len(frames)}, {delay}, {w}, {h} }},   /* {name} */")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* __OLED_GIF_H */")
    lines.append("")
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"[ok] {len(anims)} 张动图 → {OUT}（合计 {total} 字节，约 {total / 1024:.1f}KB）")


def main():
    ap = argparse.ArgumentParser(description="取模文本 → OLED 动图数组")
    ap.add_argument("source", nargs="?", help="单张模式的取模文本")
    ap.add_argument("--config", help="多张动图的 json 配置（推荐）")
    ap.add_argument("--size", type=int, default=64, help="单张模式：缩到多少像素（方形）")
    ap.add_argument("--delay", type=int, default=70, help="每帧显示时长 ms")
    ap.add_argument("--step", type=int, default=1, help="抽帧：每隔 step 帧取一帧")
    ap.add_argument("--max-frames", type=int, default=0, help="最多取几帧（0=全部）")
    ap.add_argument("--invert", action="store_true", help="取模里 1=内容时加这个")
    ap.add_argument("--keep-order", action="store_true", help="不做顺序判断")
    ap.add_argument("--peek", type=int, default=-1, help="打印第 N 帧字符画核对")
    args = ap.parse_args()

    if args.config:
        cfg = json.loads(Path(args.config).read_text(encoding="utf-8"))
        anims = []
        for i, a in enumerate(cfg["animations"]):
            packed, w, h, info = build_one(a, i, args.keep_order, args.peek if i == 0 else -1)
            print("[anim]", info)
            anims.append((a["name"], packed, w, h, int(a.get("delay", args.delay))))
        emit(anims)
        return

    if not args.source:
        ap.error("要么给 --config，要么给一个取模文本")
    anim = {"name": "OLED_ANIM0", "source": args.source, "size": [args.size, args.size],
            "step": args.step, "max_frames": args.max_frames, "invert": args.invert,
            "delay": args.delay}
    packed, w, h, info = build_one(anim, 0, args.keep_order, args.peek)
    print("[anim]", info)
    emit([("OLED_ANIM0", packed, w, h, args.delay)])


if __name__ == "__main__":
    main()
