# -*- coding: utf-8 -*-
"""按音名 DSL 生成培训板曲库 app_songs.h/.c
   DSL：c5 / #f4 之类的音名；后缀 ~ = 时值加倍；- = 休止符（占一个默认时值）
   每首歌给一个默认时值(ms)，音符表输出成 {hz, ms} —— 每个音符 4 字节。
"""
import io, os

T = r'D:\Embedded Development\STM32\wulianwang\Training'
BASE = {'c':0,'d':2,'e':4,'f':5,'g':7,'a':9,'b':11}
def hz(name):
    s = name.strip()
    sharp = False
    if s.startswith('#'):
        sharp = True; s = s[1:]
    elif '#' in s:
        sharp = True; s = s.replace('#', '')
    letter = s[0]; octv = int(s[1:])
    semi = BASE[letter] + (1 if sharp else 0)
    return int(round(440.0 * (2.0 ** ((semi - 9) / 12.0 + (octv - 4)))))

SONGS = [
 ("HAPPY", 300, "生日快乐（Happy Birthday，2016 年起公有领域）",
  "g5 g5 a5 g5 c6 b5~ g5 g5 a5 g5 d6 c6~ g5 g5 g6 e6 c6 d6 c6~ f6 f6 e6 c6 d6 c6~"),
 ("LULLABY", 350, "勃拉姆斯摇篮曲（公有领域）",
  "e5 e5 g5~ e5 e5 g5~ e5 g5 c6 b5 a5 a5 g5~ d5 e5 f5~ d5 e5 f5~ f5 e5 d5~ e5 e5 g5~ e5 e5 g5~ e5 g5 c6 b5 a5 a5 g5~ d5 e5 f5~ e5 c5 d5 c5~"),
 ("JINGLE", 280, "铃儿响叮当（Jingle Bells，公有领域）",
  "e5 e5 e5~ e5 e5 e5~ e5 g5 c5 d5 e5~ f5 f5 f5 f5 f5 e5 e5 e5~ d5 d5 e5 d5 g5~"),
 ("ELISE", 250, "致爱丽丝主题（贝多芬，公有领域）",
  "e5 d#5 e5 d#5 e5 b4 d5 c5 a4~ c4 e4 a4 b4~ e4 g#4 b4 c5~ e4 e5 d#5 e5 d#5 e5 b4 d5 c5 a4~ c4 e4 a4 b4~ e4 c5 b4 a4~"),
 ("NOKIA", 200, "诺基亚铃声（Gran Vals 片段，塔雷加，公有领域）",
  "e5 d5 #f4 #g4 #c5 b4 d4 e4 b4 a4 #c4 e4 a4"),
 ("YANKEE", 250, "扬基歌（Yankee Doodle，传统曲目，公有领域）",
  "c5 c5 d5 e5 c5 e5 d5 g4 c5 c5 d5 e5 c5~ b4~ c5 c5 d5 e5 f5 e5 d5 c5 b4 g4 a4 b4 c5~"),
 ("TWINKLE", 500, "小星星（Twinkle Twinkle，法国民谣，公有领域）",
  "c5 c5 g5 g5 a5 a5 g5~ f5 f5 e5 e5 d5 d5 c5~ g5 g5 f5 f5 e5 e5 d5~ g5 g5 f5 f5 e5 e5 d5~ c5 c5 g5 g5 a5 a5 g5~ f5 f5 e5 e5 d5 d5 c5~"),
 ("TIGER", 300, "两只老虎（Frère Jacques，法国民谣，公有领域）",
  "c5 d5 e5 c5 c5 d5 e5 c5 e5 f5 g5~ e5 f5 g5~ g5 a5 g5 f5 e5 c5 g5 a5 g5 f5 e5 c5 c5 g4 c5~ c5 g4 c5~"),
 ("ODE", 300, "欢乐颂（贝多芬《第九交响曲》主题，公有领域）",
  "e5 e5 f5 g5 g5 f5 e5 d5 c5 c5 d5 e5 e5~ d5~ e5 e5 f5 g5 g5 f5 e5 d5 c5 c5 d5 e5 d5~ c5~"),
 ("SILENT", 350, "平安夜（Silent Night，公有领域）",
  "g5~ a5 g5 e5~ g5 a5 g5 e5~ d5 d5 b5~ c6 c6 g5~ a5 a5 c6 b5 a5 g5~"),
 ("GREENSLEEVES", 350, "绿袖子（英格兰民谣，公有领域）",
  "a4 c5 d5 e5~ f5 e5 d5~ b4 g4 a4 b4~ c5 a4 a4~"),
 ("LONDON", 300, "伦敦桥（London Bridge，传统曲目，公有领域）",
  "g5 a5 g5 f5 e5 f5 g5 d5 e5 f5 e5 f5 g5 g5 a5 g5 f5 e5 f5 g5 d5~ g5 f5 e5 f5 e5 d5~"),
 ("MARY", 280, "玛丽有只小羊羔（Mary Had a Little Lamb，传统曲目，公有领域）",
  "e5 d5 c5 d5 e5 e5 e5~ d5 d5 d5~ e5 g5 g5~ e5 d5 c5 d5 e5 e5 e5 e5 d5 d5 e5 d5 c5~"),
 ("ROW", 300, "划小船（Row Row Row Your Boat，传统曲目，公有领域）",
  "c5 c5 c5 d5 e5~ e5 d5 e5 f5 g5~ c6 c6 c6 g5 g5 g5 e5 e5 e5 c5 c5 c5 g4 f4 e5 d5 c5~"),
 ("MINUET", 300, "巴赫小步舞曲主题（BWV Anh.132，公有领域）",
  "d5 g4 a4 b4 c5 d5 g4 g4 e5 c5 d5 e5 #f5 g5 g4 g4 c5 d5 c5 b4 a4 b4 c5 b4 a4 b4 c5 b4 a4 g4 #f4 g4"),
 ("CANON", 300, "卡农开头（帕赫贝尔，公有领域）",
  "#f5 e5 d5 #c5 b4 a4 b4 #c5 d5 #c5 b4 a4 g4 #f4 g4 e4"),
]

def expand(spec, base_ms):
    out = []
    for tok in spec.split():
        if tok == '-':
            out.append((0, base_ms)); continue
        ms = base_ms
        if tok.endswith('~'):
            tok = tok[:-1]; ms = base_ms * 2
        out.append((hz(tok), ms))
    return out

songs = [(n, ms, note, expand(sp, ms)) for (n, ms, note, sp) in SONGS]
total_notes = sum(len(s[3]) for s in songs)
data_bytes = total_notes * 4

# ---------- app_songs.h ----------
h = '''/**
  ******************************************************************************
  * @file    app_songs.h
  * @brief   无源蜂鸣器曲库：每个音符 {频率Hz, 时长ms}，hz=0 = 休止符
  *
  * 数据来源：全部为**公有领域**传统曲目/古典片段（儿歌、民谣、贝多芬、帕赫贝尔、
  *           塔雷加等），音符表由协会自行录入，不复制任何第三方仓库的数据文件。
  * 体积：%d 首、%d 个音符、约 %d 字节（4 B/音符）。
  ****************************************************************************** */

#ifndef __APP_SONGS_H
#define __APP_SONGS_H

#include <stdint.h>

typedef struct
{
    uint16_t hz;        /* 0 = 休止符 */
    uint16_t ms;        /* 这个音持续多少毫秒 */
} Note;

typedef struct
{
    const char   *name; /* 曲名（ASCII，直接打串口） */
    const Note   *notes;
    uint16_t      len;  /* 音符个数 */
} Song;

extern const Song    g_songs[];
extern const uint8_t g_song_num;

/* ---- 共用播放器（非阻塞：主循环里调 Songs_Task()）----
   06 蜂鸣器例程和 99 全功能的音乐页都用这一套，别再各写一份。 */
void        Songs_Play(uint8_t idx);        /* 从第 idx 首的开头开始播 */
void        Songs_Next(void);               /* 切到下一首并停在待播（不自动播） */
void        Songs_Stop(void);               /* 停止并回到开头 */
void        Songs_Pause(void);              /* 暂停：停在当前位置 */
void        Songs_Resume(void);             /* 从暂停处继续；播完/停在开头则从头播 */
void        Songs_Task(void);               /* 主循环里调：音 + 尾部静默走完推下一个 */
uint8_t     Songs_IsPlaying(void);          /* 1 = 正在播 */
uint8_t     Songs_Index(void);              /* 当前第几首（0 起） */
uint16_t    Songs_Pos(void);                /* 播到第几个音（0 起） */
uint16_t    Songs_Len(void);                /* 当前曲子的音符数 */
const char *Songs_Name(void);               /* 当前曲名（ASCII） */

#endif /* __APP_SONGS_H */
''' % (len(songs), total_notes, data_bytes)
io.open(os.path.join(T, 'App', 'Inc', 'app_songs.h'), 'w', encoding='utf-8', newline='\n').write(h)

PLAYER = r"""
/* ------------------------------ 共用播放器 ------------------------------
   非阻塞：Songs_Task() 在主循环里跑。
   断奏：每个音只"响" 90% 的时长、留 10% 静默（参考 robsoncouto/arduino-songs
   "play the note for 90% of the duration" 的做法）——音与音之间留缝，旋律才清
   晰；连着响是一串"嘟嘟嘟"，几乎没有节奏感。
   休止符（hz=0）：整段静默。 */
static uint8_t  cur_idx;
static uint8_t  playing;
static uint16_t pos;
static uint32_t note_end;      /* 当前音符（含尾部 10% 静默）结束的时刻 */

void Songs_Play(uint8_t idx)
{
    if (g_song_num == 0U)
    {
        return;
    }
    cur_idx  = (uint8_t)(idx % g_song_num);
    pos      = 0U;
    playing  = 1U;
    note_end = 0U;
    BSP_UART_Printf("[song] play %u/%u  %s\r\n",
                    (unsigned)(cur_idx + 1U), (unsigned)g_song_num, g_songs[cur_idx].name);
}

void Songs_Next(void)
{
    if (g_song_num == 0U)
    {
        return;
    }
    playing  = 0U;
    pos      = 0U;
    note_end = 0U;
    BSP_BEEP_Off();
    cur_idx = (uint8_t)((cur_idx + 1U) % g_song_num);
    BSP_UART_Printf("[song] next %u/%u  %s\r\n",
                    (unsigned)(cur_idx + 1U), (unsigned)g_song_num, g_songs[cur_idx].name);
}

void Songs_Stop(void)
{
    playing  = 0U;
    pos      = 0U;
    note_end = 0U;
    BSP_BEEP_Off();
    BSP_UART_Printf("[song] stop\r\n");
}

void Songs_Pause(void)
{
    playing  = 0U;
    note_end = 0U;                 /* 恢复时立即从下一个音开始，不等剩余间隙 */
    BSP_BEEP_Off();
    BSP_UART_Printf("[song] pause at %u/%u\r\n", (unsigned)pos, (unsigned)Songs_Len());
}

void Songs_Resume(void)
{
    if (g_song_num == 0U)
    {
        return;
    }

    if (pos >= g_songs[cur_idx].len)     /* 播完过 / 停在开头：从头来 */
    {
        pos = 0U;
    }
    playing  = 1U;
    note_end = 0U;
    BSP_UART_Printf("[song] resume %u/%u  %s  @%u\r\n",
                    (unsigned)(cur_idx + 1U), (unsigned)g_song_num,
                    g_songs[cur_idx].name, (unsigned)pos);
}

void Songs_Task(void)
{
    const Song *s;
    const Note *n;
    uint16_t    on_ms;

    if (playing == 0U)
    {
        return;
    }
    if (HAL_GetTick() < note_end)        /* 当前音的"音 + 尾部静默"还没走完 */
    {
        return;
    }

    s = &g_songs[cur_idx];
    if (pos >= s->len)
    {
        playing = 0U;                    /* 播完收工（不自动循环） */
        BSP_UART_Printf("[song] done  %s\r\n", s->name);
        return;
    }

    n = &s->notes[pos];
    if (n->hz != 0U)
    {
        on_ms = (uint16_t)((uint32_t)n->ms * 9U / 10U);   /* 只响 90% */
        if (on_ms == 0U)
        {
            on_ms = 1U;
        }
        BSP_BEEP_PlayTone(n->hz, on_ms);
    }
    else
    {
        BSP_BEEP_Off();                  /* 休止符：整段静默 */
    }
    note_end = HAL_GetTick() + n->ms;    /* 余下 10% 是音尾的静默 */
    pos++;
}

uint8_t     Songs_IsPlaying(void) { return playing; }
uint8_t     Songs_Index(void)     { return cur_idx; }
uint16_t    Songs_Pos(void)       { return pos; }
uint16_t    Songs_Len(void)       { return g_songs[cur_idx].len; }
const char *Songs_Name(void)      { return g_songs[cur_idx].name; }
"""


# ---------- app_songs.c ----------
lines = ['''/**
  ******************************************************************************
  * @file    app_songs.c
  * @brief   曲库数据（自动生成，见 Tools/make_songs.py；要加曲子改那边再重新生成）
  *
  * 每个音符 4 字节：{uint16 hz, uint16 ms}；hz=0 是休止符。
  * 全部为公有领域曲目 —— 来源见 app_songs.h 头注释。
  ****************************************************************************** */

#include "app_songs.h"
#include "bsp_beep.h"      /* 播放器直接用蜂鸣器 BSP */
#include "bsp_uart.h"      /* 串口报曲名/进度，方便验证 */

''']
for (name, ms, note, notes) in songs:
    lines.append('/* %s —— 默认 %d ms/拍，%d 个音符 */' % (note, ms, len(notes)))
    lines.append('static const Note s_%s[] = {' % name.lower())
    row = []
    for i, (f, d) in enumerate(notes):
        row.append('{%4d,%4d}' % (f, d))
        if len(row) == 6 or i == len(notes) - 1:
            lines.append('    ' + ', '.join(row) + ',')
            row = []
    lines.append('};\n')
lines.append('const Song g_songs[] = {')
for (name, ms, note, notes) in songs:
    lines.append('    { "%s", s_%s, %dU },' % (name, name.lower(), len(notes)))
lines.append('};\n')
lines.append('const uint8_t g_song_num = (uint8_t)(sizeof(g_songs) / sizeof(g_songs[0]));')
io.open(os.path.join(T, 'App', 'Src', 'app_songs.c'), 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n\n' + PLAYER + '\n')

print('生成完成：%d 首、%d 个音符、数据 %d 字节（%.2f KB）' % (len(songs), total_notes, data_bytes, data_bytes/1024))
for (name, ms, note, notes) in songs:
    print('  %-12s %3d 音符  %4d 字节  %s' % (name, len(notes), len(notes)*4, note))
