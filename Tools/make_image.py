"""把 PNG 转成 OLED 用的点阵数组（128x64，1bit，按行、最高位在左）。

用法：
    python Tools/make_image.py --demo                  # 用内置的 3 张示例图（协会名 / 机器人脸 / 棋盘）
    python Tools/make_image.py a.png b.png c.png       # 用自己的图（自动缩放居中、二值化）
    python Tools/make_image.py --demo --threshold 128  # 调二值化阈值（越大越黑）

输出：
    Hardware/Inc/oled_images.h
      #define OLED_IMG_W 128 / OLED_IMG_H 64 / OLED_IMG_COUNT n
      static const uint8_t OLED_IMG_0[] = {...};
      static const uint8_t * const OLED_IMG[] = { OLED_IMG_0, ... };

显示（铁头山羊驱动的用法）：
    OLED_SetCursor(&g_oled, 0, 0);
    OLED_DrawBitmap(&g_oled, OLED_IMG_W, OLED_IMG_H, OLED_IMG[i]);

注意：这个头文件里是"实体定义"，**只能被一个 .c 包含**（现在是 app_demo.c）。
"""

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Hardware" / "Inc" / "oled_images.h"
W, H = 128, 64
FONT_PATH = r"C:\Windows\Fonts\consolab.ttf"


def fit(img):
    """等比缩放后居中贴到 128x64 白底上（黑=点亮）"""
    img = img.convert("L")
    scale = min(W / img.width, H / img.height)
    new = img.resize((max(1, int(img.width * scale)), max(1, int(img.height * scale))), Image.LANCZOS)
    canvas = Image.new("L", (W, H), 255)
    canvas.paste(new, ((W - new.width) // 2, (H - new.height) // 2))
    return canvas


def pack(img, threshold):
    """按行打包：每行 16 字节，最高位是最左像素；黑像素=1（点亮）"""
    px = img.load()
    data = bytearray(W // 8 * H)
    for y in range(H):
        for x in range(W):
            if px[x, y] < threshold:          # 越黑越亮
                data[y * (W // 8) + x // 8] |= 0x80 >> (x % 8)
    return bytes(data)


def demo_image(idx):
    """三张内置示例图：0 协会名 / 1 机器人脸 / 2 棋盘（取模示例）"""
    img = Image.new("L", (W, H), 255)
    d = ImageDraw.Draw(img)

    if idx == 0:
        big = ImageFont.truetype(FONT_PATH, 24)
        small = ImageFont.truetype(FONT_PATH, 12)
        d.text((64, 18), "WULIAN", font=big, fill=0, anchor="mm")
        d.text((64, 40), "IOT  ASSOCIATION", font=small, fill=0, anchor="mm")
        d.rectangle([4, 4, W - 5, H - 5], outline=0)
    elif idx == 1:
        d.rectangle([14, 10, W - 15, H - 11], outline=0, width=2)      # 脸框
        d.ellipse([32, 24, 46, 38], fill=0)                            # 左眼
        d.ellipse([82, 24, 96, 38], fill=0)                            # 右眼
        d.arc([44, 34, 84, 54], start=20, end=160, fill=0, width=2)    # 笑嘴
        d.line([64, 10, 64, 18], fill=0, width=2)                      # 天线
        d.ellipse([61, 4, 67, 10], fill=0)
    else:
        for y in range(0, H, 8):
            for x in range(0, W, 8):
                if (x // 8 + y // 8) % 2 == 0:
                    d.rectangle([x, y, x + 7, y + 7], fill=0)
        d.rectangle([24, 20, 104, 44], fill=255, outline=0)
        f = ImageFont.truetype(FONT_PATH, 16)
        d.text((64, 32), "TEST", font=f, fill=0, anchor="mm")
    return img


def main():
    ap = argparse.ArgumentParser(description="PNG → OLED 点阵数组")
    ap.add_argument("images", nargs="*", help="PNG 文件（不给就用 --demo）")
    ap.add_argument("--demo", action="store_true", help="用内置 3 张示例图")
    ap.add_argument("--threshold", type=int, default=128, help="二值化阈值（默认 128）")
    args = ap.parse_args()

    if args.demo or not args.images:
        pictures = [(f"demo{i}", demo_image(i)) for i in range(3)]
    else:
        pictures = []
        for p in args.images:
            path = Path(p)
            if not path.exists():
                sys.exit(f"找不到图片：{p}")
            pictures.append((path.stem, Image.open(path)))

    lines = [
        "/* 自动生成，不要手改 —— 由 Tools/make_image.py 生成 */",
        f"/* 共 {len(pictures)} 张，每张 {W}x{H}，按行存放，最高位在左，1=点亮 */",
        "",
        "#ifndef __OLED_IMAGES_H",
        "#define __OLED_IMAGES_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define OLED_IMG_W      {W}U",
        f"#define OLED_IMG_H      {H}U",
        f"#define OLED_IMG_COUNT  {len(pictures)}U",
        "",
    ]

    for i, (name, img) in enumerate(pictures):
        data = pack(fit(img), args.threshold)
        lines.append(f"/* [{i}] {name} */")
        lines.append(f"static const uint8_t OLED_IMG_{i}[] = {{")
        for y in range(0, len(data), 16):
            row = ",".join("0x%02X" % b for b in data[y:y + 16])
            lines.append(f"    {row},")
        lines.append("};")
        lines.append("")

    lines.append("static const uint8_t * const OLED_IMG[OLED_IMG_COUNT] = {")
    lines.append("    " + ", ".join(f"OLED_IMG_{i}" for i in range(len(pictures))) + ",")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* __OLED_IMAGES_H */")
    lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"[ok] {len(pictures)} 张图 → {OUT}")
    print(f"[ok] 每张 {W}x{H} = {W // 8 * H} 字节，合计 {len(pictures) * W // 8 * H} 字节 Flash")


if __name__ == "__main__":
    main()
