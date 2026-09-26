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

## 2. 学生用：把自己的班级 / 姓名 / 要显示的整句话做成字库

```powershell
python Tools/make_font.py "物联本251周志勤|温度 %.1fC|光照 %d%%|时间 %02d:%02d"
```

- **多句话用 `|` 分开**，每句生成一个宏：`USER_TEXT_1`、`USER_TEXT_2` …
- **数字和符号自动带**：`0-9 : % . + - / 空格 C F`，不用自己写（否则运行时显示数字会变空白）
- → 覆盖 `Hardware/Inc/oled_font_user.h`
- → 顺带生成预览图 `Tools/preview_font_user.png`（**先看图，再烧板子**）

### 生成之后怎么用（三种场景）

**场景 1：只想显示自己的名字（最省事）** —— 工程里已经封好了，自检程序第 4 行就是它：

```c
BSP_OLED_ShowUserText(0, 64);      /* 显示 USER_TEXT_1，参数：起始列、这一行的基线 Y */
```

**场景 2：显示一句带数字的话**

```c
OLED_SetFont(&g_oled, &font_user);          /* 切成中文字体 */
OLED_SetCursor(&g_oled, 0, 32);             /* (列, 基线Y)：第 2 行的基线 = 2*16 = 32 */
OLED_Printf(&g_oled, USER_TEXT_2, 27.5f);   /* 温度 %.1fC */
BSP_OLED_Refresh();                         /* 改完画面必须推一次才显示 */
```

**场景 3：想显示一句新的中文**

1. 重新跑脚本，把那句话加进去（`|` 分隔）；
2. 用新编号的宏，例如 `USER_TEXT_5`。

> ⚠️ **不要在代码里直接写中文字面量**（`OLED_DrawString(&g_oled, "温度")`）：Keil 源码编码一乱，
> 屏幕上就是问号或空白。脚本生成的宏是八进制转义，不受源码编码影响。

### OLED 定位三条铁律

1. 光标 Y 是**基线，不是行的上边缘**：16 像素行高下，第 n 行的基线 = `n * 16 + 16`；
2. 一行最多 **8 个汉字**（128 ÷ 16）；
3. 改完画面要调 `BSP_OLED_Refresh()`（或 `OLED_SendBuffer(&g_oled)`）才会真正显示。

## 3. 看整版效果（多个名字排一张图）

```powershell
python Tools/make_preview_sheet.py                      # 用内置示例名单
python Tools/make_preview_sheet.py 赵权泽 黎叶 陈玉燊    # 指定名字
```

→ `Tools/字库效果总览.png`（上半屏是 8×16 英文整屏仿真，下半屏是各人姓名）

## 4. 学生操作流程（照着做）

1. 打开命令行，进到工程目录（有 `Tools` 文件夹的那一层）；
2. 敲 `python Tools/make_font.py "你的班级姓名|你要显示的句子"`（多句用 `|` 分开）；
3. 打开 `Tools/preview_font_user.png` 看一眼：字对了、没缺笔画再往下；
4. 回到 Keil 点 **Build**（F7）；
5. 插 ST-LINK 下载（下载前检查：ST-LINK 的 `3.3V` 接的是模块 SWD 座、板上没接 5V）；
6. 屏幕第 4 行就是你生成的那句话。

## 5. 注意事项

**核心一句话：屏幕上要显示的每个字，都必须在你生成字库时写进去。**
字体是一次性编进程序里的，STM32 不能像手机那样现场加载字体。

| 现象 | 原因 | 怎么办 |
|---|---|---|
| 中文不显示，英文正常 | 这个字不在字库里 | 重跑脚本，把它写进去 |
| 整行都不显示 | 光标 Y 设成了行顶（0、16…）→ 画到屏幕外 | 基线 = `行号 * 16 + 16` |
| 显示成乱码 / 问号 | 代码里直接写了中文字面量 | 用脚本生成的 `USER_TEXT_x` |
| 数字、百分号不显示 | 字库里没有这些字符 | 脚本已自动带常用符号；自己改过脚本要留意 |
| 编译报 `L6200E: Symbol ... multiply defined` | 两个 .c 同时包含了同一个字库头 | 只让一个 .c 包含（英文在 `oled.c`，中文在 `bsp_oled.c`） |
| 改了字库还是老样子 | 没重新编译 / 没重新下载 | Build（F7）后再下载 |

其它：**一个字多大**：16×16 汉字约 80 字节（48 结构 + 32 位图），32 个字约 2.5KB。
**换字体/字号**：`--font C:\Windows\Fonts\msyh.ttc`、`--size 14`。**生成物是覆盖式的**，不用清理旧文件。

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
