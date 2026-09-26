# 字库工具（Tools/）

两个脚本，只用 Windows 自带字体，不需要下载任何字库文件。

## 0. 一次性准备

1. Python 3 + Pillow：`pip install pillow`
2. 字体（系统自带，不用装）：
   - 英文：`C:\Windows\Fonts\consolab.ttf`（Consolas Bold）
   - 中文：`C:\Windows\Fonts\simhei.ttf`（黑体）

## 1. 协会用：生成 8×16 英文字库（已经生成好，一般不用再跑）

```powershell
python Tools/make_font.py --ascii
```

→ 覆盖 `Hardware/Inc/oled_font_ascii8x16.h`（95 个可打印 ASCII 字符，约 6.0KB Flash）
→ 这个字库就是工程的默认字体，`oled.c` 初始化时自己会挂上，学生不用管。

## 2. 学生用：把自己的班级 / 姓名做成字库

```powershell
python Tools/make_font.py 物联本251周志勤
```

→ 覆盖 `Hardware/Inc/oled_font_user.h`
→ 顺带生成预览图 `Tools/preview_font_user.png`（**先看图，再烧板子**）
→ 工程里已经封好一行调用，把名字显示在第 4 行：

```c
BSP_OLED_ShowUserText(0, 64);      /* 参数：起始列、这一行的基线 Y */
```

想自己在代码里用也可以，就三行：

```c
OLED_SetFont(&g_oled, &font_user);
OLED_SetCursor(&g_oled, 0, 16);
OLED_DrawString(&g_oled, USER_FONT_TEXT);
```

## 3. 看整版效果（多个名字排一张图）

```powershell
python Tools/make_preview_sheet.py                      # 用内置示例名单
python Tools/make_preview_sheet.py 赵权泽 黎叶 陈玉燊    # 指定名字
```

→ `Tools/字库效果总览.png`（上半屏是 8×16 英文整屏仿真，下半屏是各人姓名）

## 4. 学生操作流程（三句话）

1. 打开命令行，进到工程目录（有 `Tools` 文件夹的那一层）；
2. 敲 `python Tools/make_font.py 你的班级+姓名`；
3. 打开 `Tools/preview_font_user.png` 看一眼 → 没问题就重新编译、下载，屏幕上第 4 行就是你的名字。

## 5. 注意事项

- **一个字多大**：16×16 汉字约 48 字节结构 + 32 字节位图 ≈ 80 字节。5 个字约 400 字节，可以忽略。
- **屏幕上没显示字**：说明那个字不在字库里（脚本只收你写进去的字），重新跑一次脚本、重新编译即可。
- **只能被一个 .c 包含**：字库头文件里是"实体定义"，两个 .c 同时包含会报 `L6200E: Symbol ... multiply defined`。目前英文那份由 `oled.c` 包含，中文那份由 `bsp_oled.c` 包含，别在别处再包含。
- **换字体 / 换字号**：`--font C:\Windows\Fonts\msyh.ttc`、`--size 14`。
- **生成物是覆盖式的**：跑一次覆盖一次，不用手动清理旧文件。

## 6. 参数速查

| 命令 | 作用 | 输出文件 |
|---|---|---|
| `python Tools/make_font.py --ascii` | 8×16 英文字库（工程默认字体） | `Hardware/Inc/oled_font_ascii8x16.h` |
| `python Tools/make_font.py 姓名` | 学生自己的中文班级/姓名 | `Hardware/Inc/oled_font_user.h` |
| `python Tools/make_preview_sheet.py 名字...` | 效果总览图 | `Tools/字库效果总览.png` |

## 7. 字模格式（改动前必读）

生成的文件要能被铁头山羊那套 `oled.c` 直接认，格式必须一致：

- 位图**按行存放**，每行 `ceil(宽/8)` 字节，**最高位是最左边像素**；
- 每个字形是一整格（英文 8×16 = 16 字节；中文 16×16 = 32 字节）；
- 光标 Y 是**基线**，格子底边贴在基线上（`BByoff0y = 0`）；
- 行高 = `FontSize` = `FBBy` = 格子高度，用于换行与 `OLED_GetFontHeight()`；
- 基线位置：英文第 13 行（下面留 3 行给 g/p/y），中文第 14 行（汉字不下伸，字身撑满 16 像素）。
