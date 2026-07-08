/**
 * @file    bsp_oled.h
 * @brief   1.3" OLED 驱动（SH1106/SSD1306 兼容，页寻址模式，软件 I2C）
 *
 * 注意：1.3 寸 OLED 多为 SH1106 控制器（132x64，可视 128 列居中，列偏移 2）；
 * 0.96 寸多为 SSD1306（列偏移 0）。通过 OLED_COL_OFFSET 适配，默认按 SH1106。
 */
#ifndef BSP_OLED_H
#define BSP_OLED_H

#include <stdint.h>

#define OLED_WIDTH        128
#define OLED_HEIGHT       64
#define OLED_PAGES        (OLED_HEIGHT / 8)   /* 8 页 */
#define OLED_COL_OFFSET   2                   /* SH1106=2, SSD1306 改为 0 */
#define OLED_I2C_ADDR     0x78                /* 0x3C << 1 */

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Display(void);                       /* 显存刷到屏幕 */
void OLED_DisplayOn(void);                     /* 退出低功耗/亮屏 */
void OLED_DisplayOff(void);                    /* 熄屏（仅关闭显示，MCU 全速运行） */

/* 在显存绘制（需调用 OLED_Display 才显示）。row=页(0..7)，col=像素列(0..127) */
void OLED_ClearBuf(void);
void OLED_ShowChar(uint8_t page, uint8_t col, char ch);
void OLED_ShowString(uint8_t page, uint8_t col, const char *str);

/* ---------------- 图形/大字号绘制（像素坐标 x,y；0<=x<128, 0<=y<64） ----------------
 * on=1 点亮、on=0 熄灭。这些原语直接操作显存，配合 OLED_Display 刷新。          */
void    OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t on);
void    OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t on);
void    OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);   /* 空心边框 */
void    OLED_DrawHLine(uint8_t x, uint8_t y, uint8_t w);
void    OLED_DrawVLine(uint8_t x, uint8_t y, uint8_t h);

/* 缩放字符/字符串：scale=1 → 6x8，scale=2 → 12x16，scale=3 → 18x24。
 * inv=1 表示以"熄灭色"绘制笔画（用于在已填充的白底上写黑字，如反白标题栏）。 */
void    OLED_DrawCharEx(uint8_t x, uint8_t y, char ch, uint8_t scale, uint8_t inv);
void    OLED_DrawStringEx(uint8_t x, uint8_t y, const char *str, uint8_t scale, uint8_t inv);
uint8_t OLED_StrWidth(const char *str, uint8_t scale);   /* 像素宽度，便于居中 */

#endif /* BSP_OLED_H */
