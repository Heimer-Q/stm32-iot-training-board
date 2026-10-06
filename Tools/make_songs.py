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
 ("TWINKLE", 300, "小星星（Twinkle Twinkle，法国民谣，公有领域）",
  "c5 c5 g5 g5 a5 a5 g5~ f5 f5 e5 e5 d5 d5 c5~ g5 g5 f5 f5 e5 e5 d5~ g5 g5 f5 f5 e5 e5 d5~ c5 c5 g5 g5 a5 a5 g5~ f5 f5 e5 e5 d5 d5 c5~"),
 ("TIGER", 300, "两只老虎（Frère Jacques，法国民谣，公有领域）",
  "c5 d5 e5 c5 c5 d5 e5 c5 e5 f5 g5~ e5 f5 g5~ g5 a5 g5 f5 e5 c5 g5 a5 g5 f5 e5 c5 c5 g4 c5~ c5 g4 c5~"),
 ("HAPPY", 300, "生日快乐（Happy Birthday，2016 年起公有领域）",
  "g5 g5 a5 g5 c6 b5~ g5 g5 a5 g5 d6 c6~ g5 g5 g6 e6 c6 d6 c6~ f6 f6 e6 c6 d6 c6~"),
 ("ODE", 300, "欢乐颂（贝多芬《第九交响曲》主题，公有领域）",
  "e5 e5 f5 g5 g5 f5 e5 d5 c5 c5 d5 e5 e5~ d5~ e5 e5 f5 g5 g5 f5 e5 d5 c5 c5 d5 e5 d5~ c5~"),
 ("LULLABY", 350, "勃拉姆斯摇篮曲（公有领域）",
  "e5 e5 g5~ e5 e5 g5~ e5 g5 c6 b5 a5 a5 g5~ d5 e5 f5~ d5 e5 f5~ f5 e5 d5~ e5 e5 g5~ e5 e5 g5~ e5 g5 c6 b5 a5 a5 g5~ d5 e5 f5~ e5 c5 d5 c5~"),
 ("JINGLE", 280, "铃儿响叮当（Jingle Bells，公有领域）",
  "e5 e5 e5~ e5 e5 e5~ e5 g5 c5 d5 e5~ f5 f5 f5 f5 f5 e5 e5 e5~ d5 d5 e5 d5 g5~"),
 ("SILENT", 350, "平安夜（Silent Night，公有领域）",
  "g5~ a5 g5 e5~ g5 a5 g5 e5~ d5 d5 b5~ c6 c6 g5~ a5 a5 c6 b5 a5 g5~"),
 ("GREENSLEEVES", 350, "绿袖子（英格兰民谣，公有领域）",
  "a4 c5 d5 e5~ f5 e5 d5~ b4 g4 a4 b4~ c5 a4 a4~"),
 ("ELISE", 250, "致爱丽丝主题（贝多芬，公有领域）",
  "e5 d#5 e5 d#5 e5 b4 d5 c5 a4~ c4 e4 a4 b4~ e4 g#4 b4 c5~ e4 e5 d#5 e5 d#5 e5 b4 d5 c5 a4~ c4 e4 a4 b4~ e4 c5 b4 a4~"),
 ("NOKIA", 200, "诺基亚铃声（Gran Vals 片段，塔雷加，公有领域）",
  "e5 d5 #f4 #g4 #c5 b4 d4 e4 b4 a4 #c4 e4 a4"),
 ("YANKEE", 250, "扬基歌（Yankee Doodle，传统曲目，公有领域）",
  "c5 c5 d5 e5 c5 e5 d5 g4 c5 c5 d5 e5 c5~ b4~ c5 c5 d5 e5 f5 e5 d5 c5 b4 g4 a4 b4 c5~"),
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

#endif /* __APP_SONGS_H */
''' % (len(songs), total_notes, data_bytes)
io.open(os.path.join(T, 'App', 'Inc', 'app_songs.h'), 'w', encoding='utf-8', newline='\n').write(h)

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
io.open(os.path.join(T, 'App', 'Src', 'app_songs.c'), 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')

print('生成完成：%d 首、%d 个音符、数据 %d 字节（%.2f KB）' % (len(songs), total_notes, data_bytes, data_bytes/1024))
for (name, ms, note, notes) in songs:
    print('  %-12s %3d 音符  %4d 字节  %s' % (name, len(notes), len(notes)*4, note))
