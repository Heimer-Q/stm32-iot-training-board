"""把字库效果渲成一张总览图，给会长 / 学生看效果用。

用法：
    python Tools/make_preview_sheet.py                      # 用内置的示例名单
    python Tools/make_preview_sheet.py 赵权泽 黎叶 陈玉燊    # 指定要预览的名字

输出：
    Tools/字库效果总览.png

说明：图里的每个字都是从"打包后的字模字节"还原出来的，和 OLED 上点亮后的像素一致。
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_font as mf

ROOT = Path(__file__).resolve().parent.parent
SCALE = 4
BG = (10, 12, 16)
FG = (255, 255, 255)
DIM = (140, 150, 160)


def render_text(glyph_map, text, cell_w, cell_h):
    """把字符串按字模渲染成 (宽 x 高) 的灰度图。"""
    img = Image.new("L", (cell_w * len(text), cell_h), 0)
    x = 0
    for ch in text:
        data = glyph_map.get(ord(ch))
        if data is not None:
            stride = (cell_w + 7) // 8
            for y in range(cell_h):
                for xx in range(cell_w):
                    if data[y * stride + xx // 8] & (0x80 >> (xx % 8)):
                        img.putpixel((x + xx, y), 255)
        x += cell_w
    return img


def build_font_map(font_path, text, cell_w, cell_h, baseline_row):
    chars = mf.unique_chars(text)
    size = mf.pick_size(font_path, chars, cell_w, cell_h, baseline_row)
    font = ImageFont.truetype(font_path, size)
    return {ord(ch): mf.pack(mf.render_cell(font, ch, cell_w, cell_h, baseline_row)) for ch in chars}, size


def paste_white(canvas, img, xy):
    """把灰度字模贴到深色画布上（白字）。"""
    white = Image.new("RGB", img.size, FG)
    canvas.paste(white, xy, img)


def main():
    names = sys.argv[1:] or ["赵权泽", "黎叶", "陈玉燊", "黄小娘", "张誉馨", "欧佳萍", "伍弘毅", "石政铨"]

    ascii_font = mf.find_font(mf.ASCII_FONT_CANDIDATES)
    cjk_font = mf.find_font(mf.CJK_FONT_CANDIDATES)

    ascii_lines = ["SELFTEST  v0.2", "OLED : OK", "KEY  : 0 0 0", "TEMP 27.5C"]
    ascii_map, ascii_size = build_font_map(
        ascii_font, "".join(ascii_lines), 8, 16, mf.BASELINE_ROW_ASCII)

    name_maps = []
    for name in names:
        gmap, size = build_font_map(cjk_font, name, 16, 16, mf.BASELINE_ROW_CJK)
        name_maps.append((name, gmap, size))

    title_font = ImageFont.truetype(cjk_font, 26)
    label_font = ImageFont.truetype(cjk_font, 18)
    note_font = ImageFont.truetype(cjk_font, 16)

    width = 900
    screen_w, screen_h = 128 * SCALE, 64 * SCALE
    height = 150 + screen_h + 90 + len(name_maps) * 90 + 90
    sheet = Image.new("RGB", (width, height), BG)
    draw = ImageDraw.Draw(sheet)

    draw.text((40, 28), "物联网协会 2026 培训板 · OLED 字库效果预览", font=title_font, fill=FG)
    draw.text((40, 66), "图内像素 = 实际下发给单片机的字模字节，与 OLED 点亮后一致", font=note_font, fill=DIM)

    # ① 8x16 英文：整屏 128x64 仿真
    y = 110
    draw.text((40, y), f"① 8x16 英文（工程默认字体，字号 {ascii_size}，95 个字符 ≈ 6.0KB Flash）",
              font=label_font, fill=FG)
    y += 34
    screen = Image.new("L", (128, 64), 0)
    for row, line in enumerate(ascii_lines):
        screen.paste(render_text(ascii_map, line, 8, 16), (0, row * 16))
    screen = screen.resize((screen_w, screen_h), Image.NEAREST)
    draw.rectangle([38, y - 2, 40 + screen_w + 2, y + screen_h + 2], outline=(70, 80, 90))
    paste_white(sheet, screen, (40, y))
    y += screen_h + 60

    # ② 16x16 中文：姓名逐个预览
    draw.text((40, y), f"② 16x16 中文（学生自己生成：python Tools/make_font.py 姓名）",
              font=label_font, fill=FG)
    y += 40
    for name, gmap, size in name_maps:
        img = render_text(gmap, name, 16, 16).resize((16 * SCALE * len(name), 16 * SCALE), Image.NEAREST)
        paste_white(sheet, img, (60, y))
        draw.text((60 + img.width + 24, y + 24), f"{name}   字号 {size}",
                  font=note_font, fill=DIM)
        y += 90

    out = ROOT / "Tools" / "字库效果总览.png"
    sheet.save(out)
    print(f"[ok] 已生成 {out}  ({sheet.width}x{sheet.height})")


if __name__ == "__main__":
    main()
