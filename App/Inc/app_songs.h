/**
  ******************************************************************************
  * @file    app_songs.h
  * @brief   无源蜂鸣器曲库：每个音符 {频率Hz, 时长ms}，hz=0 = 休止符
  *
  * 数据来源：全部为**公有领域**传统曲目/古典片段（儿歌、民谣、贝多芬、帕赫贝尔、
  *           塔雷加等），音符表由协会自行录入，不复制任何第三方仓库的数据文件。
  * 体积：16 首、430 个音符、约 1720 字节（4 B/音符）。
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
void        Songs_Pause(void);      /* 暂停：停在当前位置 */
void        Songs_Resume(void);     /* 从暂停处继续；播完/停在开头则从头播 */
void        Songs_Task(void);               /* 主循环里调：上一个音放完就推下一个 */
uint8_t     Songs_IsPlaying(void);          /* 1 = 正在播 */
uint8_t     Songs_Index(void);              /* 当前第几首（0 起） */
uint16_t    Songs_Pos(void);                /* 播到第几个音（0 起） */
uint16_t    Songs_Len(void);                /* 当前曲子的音符数 */
const char *Songs_Name(void);               /* 当前曲名（ASCII） */

#endif /* __APP_SONGS_H */
