"""按铁头山羊的 BDF 字体格式，从系统字体生成 OLED 字库（.h）。

两种用法
--------
1) 协会用：生成 8x16 英文字库（工程默认字体）
       python Tools/make_font.py --ascii
   → Hardware/Inc/oled_font_ascii8x16.h  ，字体对象 font_ascii8x16

2) 学生用：把自己的班级 / 姓名 / 要显示的整句话做成字库
       python Tools/make_font.py "哲学本263陈桂林"
       python Tools/make_font.py "温度 %.1fC|光照 %d%%|时间 %02d:%02d"
     （多句话用 | 分开，每句会生成一个宏 USER_TEXT_1 / USER_TEXT_2 …）
   → Hardware/Inc/oled_font_user.h       ，字体对象 font_user
     同时生成文本宏（UTF-8 转义写法，避免源码编码问题），用法：
       OLED_SetFont(&g_oled, &font_user);
       OLED_SetCursor(&g_oled, 0, 16);
       OLED_DrawString(&g_oled, USER_TEXT_1);          // 纯文字
       OLED_Printf(&g_oled, USER_TEXT_2, 27.5f);       // 带数字

参数
----
    --font PATH   指定 TTF/TTC；不给就自动找（英文默认 Consolas Bold，中文默认黑体）
    --size N      指定字号；不给就自动挑能塞进网格的最大字号

格式约定（与铁头山羊 oled.c 完全一致，不要改）
----------------------------------------------
    * 位图按行存放：每行 ceil(宽/8) 字节，行的最高位是最左边那个像素；
    * 每个字形是一整格（英文 8x16 = 16 字节；中文 16x16 = 32 字节）；
    * 光标 Y 是基线，格子底边贴在基线上（BByoff0y = 0）；
    * 行高 = FontSize = FBBy = 格子高度，用于换行和 OLED_GetFontHeight()。
"""

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
INC_DIR = ROOT / "Hardware" / "Inc"

ASCII_FONT_CANDIDATES = ["consolab.ttf", "consola.ttf", "courbd.ttf", "arialbd.ttf"]
CJK_FONT_CANDIDATES = ["simhei.ttf", "msyh.ttc", "simsun.ttc", "simkai.ttf"]
THRESHOLD = 96          # 灰度阈值：越大字越细

# 中文字库默认附带这些"常用符号"：不然运行时显示的数字/百分号会查不到字模变成空白
AUTO_SYMBOLS = "0123456789:%.+-/ CF"

# 基线在格子里的行号：基线之上放字身，基线之下留给 g/p/y 的下伸部分。
# 英文用 13（下面留 3 行），中文用 14（汉字几乎不下伸，把字身撑满 16 像素更清楚）。
BASELINE_ROW_ASCII = 13
BASELINE_ROW_CJK = 14


def find_font(candidates):
    for name in candidates:
        path = Path(r"C:\Windows\Fonts") / name
        if path.exists():
            return str(path)
    return None


def measure(font, ch, cell_w, cell_h, baseline_row):
    """量出墨迹相对基线的范围 (x0, y0, x1, y1)；y 为负表示在基线之上。空白字符返回 None。

    画布给得足够大（3 倍格子高），所以不会因为贴着边被裁掉而量错。
    """
    origin_x, origin_y = cell_w * 2, cell_h * 2
    canvas = Image.new("L", (cell_w * 4, cell_h * 4), 0)
    ImageDraw.Draw(canvas).text((origin_x, origin_y), ch, font=font, fill=255, anchor="ls")
    box = canvas.getbbox()
    if box is None:
        return None
    x0, y0, x1, y1 = box
    return (x0 - origin_x, y0 - origin_y, x1 - origin_x, y1 - origin_y)


def pick_size(font_path, chars, cell_w, cell_h, baseline_row):
    """从大到小试，挑第一个所有字符都塞得进网格的字号。"""
    for size in range(cell_h + 2, 5, -1):
        font = ImageFont.truetype(font_path, size)
        for ch in chars:
            box = measure(font, ch, cell_w, cell_h, baseline_row)
            if box is None:
                continue
            x0, y0, x1, y1 = box
            # 基线之上要放得下 -y0，基线之下要放得下 y1
            if (x1 - x0) > cell_w or (-y0) > baseline_row or y1 > (cell_h - baseline_row):
                break
        else:
            return size
    raise SystemExit("没有可用字号：这个字体塞不进网格，换一个字体或加大网格")


def render_cell(font, ch, cell_w, cell_h, baseline_row=13):
    """把一个字渲染进 (cell_w x cell_h) 的格子：水平居中，基线固定在 baseline_row。"""
    cell = Image.new("L", (cell_w, cell_h), 0)
    box = measure(font, ch, cell_w, cell_h, baseline_row)
    if box is None:
        return cell                       # 空格：整格空白
    x0, _, x1, _ = box
    dx = (cell_w - (x1 - x0)) // 2 - x0   # 墨迹在格子里水平居中（x0 是相对基点的偏移）
    ImageDraw.Draw(cell).text((dx, baseline_row), ch, font=font, fill=255, anchor="ls")
    return cell


def pack(cell):
    """按行打包：每行 ceil(w/8) 字节，最高位 = 最左像素。"""
    w, h = cell.size
    stride = (w + 7) // 8
    px = cell.load()
    data = bytearray(stride * h)
    for y in range(h):
        for x in range(w):
            if px[x, y] >= THRESHOLD:
                data[y * stride + x // 8] |= 0x80 >> (x % 8)
    return bytes(data)


def c_escape(text):
    """把文本转成 UTF-8 的八进制转义字符串。

    用八进制（固定 3 位）而不是 \\x：\\x 转义是贪婪的，后面紧跟数字/字母时会被吃掉，
    例如 "温度C" 的最后一个字节后面接 C，\\x 写法就会解析错。
    """
    return "".join("\\%03o" % b for b in text.encode("utf-8"))


def build_header(symbol, font_name, font_path, size, chars, cell_w, cell_h, baseline_row, texts=None):
    font = ImageFont.truetype(font_path, size)
    glyphs = [(ord(ch), ch, pack(render_cell(font, ch, cell_w, cell_h, baseline_row))) for ch in chars]

    L = []
    L.append("/**")
    L.append("  ******************************************************************************")
    L.append(f"  * @file    {symbol}.h")
    L.append("  * @brief   自动生成，不要手改 —— 由 Tools/make_font.py 生成")
    L.append(f"  * 字体   ：{Path(font_path).name}  字号 {size}  网格 {cell_w}x{cell_h}")
    L.append(f"  * 字符数 ：{len(chars)} 个，占用 Flash 约 {len(chars) * (cell_h * ((cell_w + 7) // 8) + 48)} 字节")
    L.append(f"  * 生成命令：python Tools/make_font.py --ascii" if symbol == "font_ascii8x16"
             else f"  * 生成命令：python Tools/make_font.py \"{''.join(chars)}\"")
    L.append("  ******************************************************************************")
    L.append("  */")
    L.append("")
    L.append(f"#ifndef __{symbol.upper()}_H")
    L.append(f"#define __{symbol.upper()}_H")
    L.append("")
    L.append('#include "oled_font.h"')
    L.append("")

    if texts:
        L.append("/* 直接给 OLED_DrawString / OLED_Printf 用的文本（UTF-8 八进制转义，不受源码编码影响）")
        L.append("   注意：屏幕上要显示的字，必须出现在生成字库时写的那几句话里 */")
        for i, t in enumerate(texts, 1):
            L.append(f'#define USER_TEXT_{i} "{c_escape(t)}"   /* {t} */')
        L.append("")
        L.append("#define USER_FONT_TEXT USER_TEXT_1   /* 兼容旧写法：默认显示第一句 */")
        L.append("")

    L.append("/* ---- 字形位图 ---- */")
    for code, ch, data in glyphs:
        body = ",".join("0x%02X" % b for b in data)
        label = ch if ch.isprintable() and ord(ch) < 128 else "U+%04X %s" % (code, ch)
        L.append(f"static const uint8_t {symbol}_Bitmap_{code:04X}[] = {{{body}}};   /* {label} */")
    L.append("")

    L.append(f"static const uint32_t {symbol}_Map[] = {{")
    L.append("    " + ",".join(str(code) for code, _, _ in glyphs) + ",")
    L.append("};")
    L.append("")

    L.append(f"static const Glyph_TypeDef {symbol}_Glyphs[] = {{")
    for code, ch, data in glyphs:
        label = ch if ord(ch) < 128 and ch.isprintable() else "U+%04X" % code
        L.append(f"    {{   /* {label} */")
        L.append("        .Name = 0,")
        L.append(f"        .Encoding = {code},")
        L.append("        .Swx0 = 1000, .Swy0 = 0,")
        L.append(f"        .Dwx0 = {cell_w}, .Dwy0 = 0,")
        L.append("        .Swx1 = 1000, .Swy1 = 0,")
        L.append(f"        .Dwx1 = {cell_w}, .Dwy1 = 0,")
        L.append("        .VVectorXoff = 0, .VVectorYoff = 0,")
        L.append(f"        .BBw = {cell_w}, .BBh = {cell_h},")
        L.append("        .BBxoff0x = 0, .BByoff0y = 0,")
        L.append(f"        .nBytes = {len(data)},")
        L.append(f"        .Bitmap = {symbol}_Bitmap_{code:04X},")
        L.append("    },")
    L.append("};")
    L.append("")

    L.append(f"const Font_TypeDef {symbol} =")
    L.append("{")
    L.append('    .SpecVersion = "2.1",')
    L.append(f'    .FontName = "{font_name}",')
    L.append("    .ContentVersion = 1,")
    L.append("    .MetricsSet = 0,")
    L.append(f"    .FontSize = {cell_h},      /* 行高：QLED_GetFontHeight 返回的就是它 */".replace("QLED", "OLED"))
    L.append("    .Xres = 72, .Yres = 72,")
    L.append(f"    .FBBx = {cell_w}, .FBBy = {cell_h},")
    L.append("    .FBBXoff = 0, .FBBYoff = 0,")
    L.append(f"    .nChars = {len(chars)},")
    L.append(f"    .Map = {symbol}_Map,")
    L.append(f"    .Glyphs = {symbol}_Glyphs,")
    L.append("};")
    L.append("")
    L.append(f"#endif /* __{symbol.upper()}_H */")
    L.append("")
    return "\n".join(L)


def unique_chars(text):
    seen = []
    for ch in text:
        if ch not in seen:
            seen.append(ch)
    return seen


def render_preview(glyph_map, lines, cell_w, cell_h, scale=3):
    """把打包后的字节还原成 128x64 的图 —— 屏幕上会显示成什么样，图里就是什么样。"""
    canvas = Image.new("L", (128, cell_h * max(1, len(lines))), 0)
    for row, line in enumerate(lines):
        x = 0
        for ch in line:
            data = glyph_map.get(ord(ch))
            if data is None:
                x += cell_w
                continue
            stride = (cell_w + 7) // 8
            for y in range(cell_h):
                for xx in range(cell_w):
                    if data[y * stride + xx // 8] & (0x80 >> (xx % 8)):
                        canvas.putpixel((x + xx, row * cell_h + y), 255)
            x += cell_w
    return canvas.resize((canvas.width * scale, canvas.height * scale), Image.NEAREST)


def main():
    ap = argparse.ArgumentParser(description="生成铁头山羊格式的 OLED 字库头文件")
    ap.add_argument("text", nargs="?", help="要生成的字（学生写自己的班级/姓名）")
    ap.add_argument("--ascii", action="store_true", help="生成 8x16 英文字库（协会用）")
    ap.add_argument("--font", help="TTF/TTC 路径，不给就自动找")
    ap.add_argument("--size", type=int, help="字号，不给就自动挑最大的")
    args = ap.parse_args()

    if args.ascii:
        chars = [chr(c) for c in range(0x20, 0x7F)]
        cell_w, cell_h = 8, 16
        symbol = "font_ascii8x16"
        font_path = args.font or find_font(ASCII_FONT_CANDIDATES)
        out = INC_DIR / "oled_font_ascii8x16.h"
        font_name = "ASCII 8x16"
        extra = None
        preview_lines = ["SELFTEST  v0.2", "OLED : OK", "KEY  : 0 0 0", "TEMP 27.5C"]
        texts = None
    else:
        if not args.text:
            ap.error("要么加 --ascii 生成英文，要么写一段文字（例如：python Tools/make_font.py 陈桂林）")
        texts = [t for t in args.text.split("|") if t != ""]
        chars = unique_chars("".join(texts) + AUTO_SYMBOLS)   # 自动补常用数字与符号
        cell_w, cell_h = 16, 16
        symbol = "font_user"
        font_path = args.font or find_font(CJK_FONT_CANDIDATES)
        out = INC_DIR / "oled_font_user.h"
        font_name = "USER 16x16"
        per_line = 128 // cell_w
        preview_lines = []
        for t in texts:
            preview_lines += [t[i:i + per_line] for i in range(0, len(t), per_line)]

    if not font_path or not Path(font_path).exists():
        raise SystemExit("找不到字体文件，用 --font 指定，例如 --font C:\\Windows\\Fonts\\simhei.ttf")

    baseline_row = BASELINE_ROW_ASCII if args.ascii else BASELINE_ROW_CJK
    size = args.size or pick_size(font_path, chars, cell_w, cell_h, baseline_row)
    header = build_header(symbol, font_name, font_path, size, chars, cell_w, cell_h, baseline_row, texts)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(header, encoding="utf-8")

    glyph_map = {ord(ch): pack(render_cell(ImageFont.truetype(font_path, size), ch, cell_w, cell_h, baseline_row))
                 for ch in chars}
    preview = render_preview(glyph_map, preview_lines, cell_w, cell_h)
    preview_path = ROOT / "Tools" / f"preview_{symbol}.png"
    preview.save(preview_path)

    flash = len(chars) * (cell_h * ((cell_w + 7) // 8) + 48)
    print(f"[ok] 字体 {Path(font_path).name}  字号 {size}  网格 {cell_w}x{cell_h}")
    print(f"[ok] 字符 {len(chars)} 个，占 Flash 约 {flash} 字节")
    print(f"[ok] 已写出 {out}")
    print(f"[ok] 预览图 {preview_path}")
    if args.ascii:
        print('用法：OLED_SetFont(&g_oled, &font_ascii8x16);')
    else:
        print("用法：OLED_SetFont(&g_oled, &font_user);")
        print('      OLED_SetCursor(&g_oled, 0, 16);')
        for i, t in enumerate(texts, 1):
            if "%" in t:
                print(f"      OLED_Printf(&g_oled, USER_TEXT_{i}, ...);   /* {t} */")
            else:
                print(f"      OLED_DrawString(&g_oled, USER_TEXT_{i});   /* {t} */")


if __name__ == "__main__":
    try:
        main()
    except SystemExit as e:
        print(e)
        sys.exit(1)
