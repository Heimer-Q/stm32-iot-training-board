/**
  ******************************************************************************
  * @file    font_user.h
  * @brief   自动生成，不要手改 —— 由 Tools/make_font.py 生成
  * 字体   ：simhei.ttf  字号 16  网格 16x16
  * 字符数 ：5 个，占用 Flash 约 400 字节
  * 生成命令：python Tools/make_font.py "物联网协会"
  ******************************************************************************
  */

#ifndef __FONT_USER_H
#define __FONT_USER_H

#include "oled_font.h"

/* 直接给 OLED_DrawString 用的文本（UTF-8 转义，不受源码编码影响） */
#define USER_FONT_TEXT "\xE7\x89\xA9\xE8\x81\x94\xE7\xBD\x91\xE5\x8D\x8F\xE4\xBC\x9A"   /* 物联网协会 */

/* ---- 字形位图 ---- */
static const uint8_t font_user_Bitmap_7269[] = {0x00,0x00,0x08,0x40,0x08,0xC0,0x68,0xFE,0x6D,0xFE,0x7F,0x6A,0x4B,0x6A,0x48,0x5A,0x08,0xDA,0x1F,0x96,0x79,0xB6,0x08,0x36,0x08,0x66,0x08,0xC6,0x09,0x9C,0x08,0x00};   /* U+7269 物 */
static const uint8_t font_user_Bitmap_8054[] = {0x00,0x00,0x00,0x88,0x7F,0xC8,0x24,0x58,0x25,0xFE,0x3C,0x30,0x24,0x20,0x24,0x20,0x3D,0xFE,0x3D,0xFE,0x24,0x30,0x3E,0x70,0x7C,0x48,0x44,0xCC,0x05,0x86,0x04,0x00};   /* U+8054 联 */
static const uint8_t font_user_Bitmap_7F51[] = {0x00,0x00,0x00,0x00,0x7F,0xFE,0x60,0x06,0x61,0x1E,0x7B,0x96,0x6E,0xD6,0x66,0x76,0x66,0x26,0x66,0x76,0x6F,0x76,0x68,0xDE,0x79,0x86,0x60,0x06,0x60,0x1E,0x20,0x08};   /* U+7F51 网 */
static const uint8_t font_user_Bitmap_534F[] = {0x00,0x00,0x10,0x40,0x30,0x40,0x30,0x40,0x33,0xF8,0x7D,0xF8,0x30,0xC8,0x32,0xC8,0x32,0xCE,0x36,0x8A,0x34,0x9A,0x31,0x98,0x33,0x18,0x33,0x38,0x36,0x30,0x10,0x00};   /* U+534F 协 */
static const uint8_t font_user_Bitmap_4F1A[] = {0x00,0x00,0x01,0x80,0x01,0x80,0x02,0xC0,0x04,0x60,0x18,0x38,0x3F,0xFE,0x67,0xE2,0x00,0x00,0x3F,0xFC,0x03,0x00,0x07,0x20,0x0C,0x30,0x18,0x38,0x3F,0xFC,0x00,0x00};   /* U+4F1A 会 */

static const uint32_t font_user_Map[] = {
    29289,32852,32593,21327,20250,
};

static const Glyph_TypeDef font_user_Glyphs[] = {
    {   /* U+7269 */
        .Name = 0,
        .Encoding = 29289,
        .Swx0 = 1000, .Swy0 = 0,
        .Dwx0 = 16, .Dwy0 = 0,
        .Swx1 = 1000, .Swy1 = 0,
        .Dwx1 = 16, .Dwy1 = 0,
        .VVectorXoff = 0, .VVectorYoff = 0,
        .BBw = 16, .BBh = 16,
        .BBxoff0x = 0, .BByoff0y = 0,
        .nBytes = 32,
        .Bitmap = font_user_Bitmap_7269,
    },
    {   /* U+8054 */
        .Name = 0,
        .Encoding = 32852,
        .Swx0 = 1000, .Swy0 = 0,
        .Dwx0 = 16, .Dwy0 = 0,
        .Swx1 = 1000, .Swy1 = 0,
        .Dwx1 = 16, .Dwy1 = 0,
        .VVectorXoff = 0, .VVectorYoff = 0,
        .BBw = 16, .BBh = 16,
        .BBxoff0x = 0, .BByoff0y = 0,
        .nBytes = 32,
        .Bitmap = font_user_Bitmap_8054,
    },
    {   /* U+7F51 */
        .Name = 0,
        .Encoding = 32593,
        .Swx0 = 1000, .Swy0 = 0,
        .Dwx0 = 16, .Dwy0 = 0,
        .Swx1 = 1000, .Swy1 = 0,
        .Dwx1 = 16, .Dwy1 = 0,
        .VVectorXoff = 0, .VVectorYoff = 0,
        .BBw = 16, .BBh = 16,
        .BBxoff0x = 0, .BByoff0y = 0,
        .nBytes = 32,
        .Bitmap = font_user_Bitmap_7F51,
    },
    {   /* U+534F */
        .Name = 0,
        .Encoding = 21327,
        .Swx0 = 1000, .Swy0 = 0,
        .Dwx0 = 16, .Dwy0 = 0,
        .Swx1 = 1000, .Swy1 = 0,
        .Dwx1 = 16, .Dwy1 = 0,
        .VVectorXoff = 0, .VVectorYoff = 0,
        .BBw = 16, .BBh = 16,
        .BBxoff0x = 0, .BByoff0y = 0,
        .nBytes = 32,
        .Bitmap = font_user_Bitmap_534F,
    },
    {   /* U+4F1A */
        .Name = 0,
        .Encoding = 20250,
        .Swx0 = 1000, .Swy0 = 0,
        .Dwx0 = 16, .Dwy0 = 0,
        .Swx1 = 1000, .Swy1 = 0,
        .Dwx1 = 16, .Dwy1 = 0,
        .VVectorXoff = 0, .VVectorYoff = 0,
        .BBw = 16, .BBh = 16,
        .BBxoff0x = 0, .BByoff0y = 0,
        .nBytes = 32,
        .Bitmap = font_user_Bitmap_4F1A,
    },
};

const Font_TypeDef font_user =
{
    .SpecVersion = "2.1",
    .FontName = "USER 16x16",
    .ContentVersion = 1,
    .MetricsSet = 0,
    .FontSize = 16,      /* 行高：OLED_GetFontHeight 返回的就是它 */
    .Xres = 72, .Yres = 72,
    .FBBx = 16, .FBBy = 16,
    .FBBXoff = 0, .FBBYoff = 0,
    .nChars = 5,
    .Map = font_user_Map,
    .Glyphs = font_user_Glyphs,
};

#endif /* __FONT_USER_H */
