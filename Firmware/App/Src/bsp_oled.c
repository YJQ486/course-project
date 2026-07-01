/**
 * @file    bsp_oled.c
 * @brief   1.3" OLED 驱动实现（SH1106/SSD1306 兼容，页寻址，软件 I2C）
 */
#include "bsp_oled.h"
#include "bsp_soft_i2c.h"
#include "oled_font.h"
#include "app_config.h"   /* 为 HAL_Delay 等 HAL 声明 */
#include <string.h>

/* 本地显存：8 页 x 128 列 */
static uint8_t s_gram[OLED_PAGES][OLED_WIDTH];

/* 控制字节：0x00=后跟命令，0x40=后跟数据 */
static void oled_write_cmd(uint8_t cmd)
{
    SoftI2C_Start();
    SoftI2C_WriteByte(OLED_I2C_ADDR);
    SoftI2C_WriteByte(0x00);
    SoftI2C_WriteByte(cmd);
    SoftI2C_Stop();
}

void OLED_Init(void)
{
    SoftI2C_Init();
    HAL_Delay(100);                 /* 上电稳定 */

    oled_write_cmd(0xAE);           /* display off */
    oled_write_cmd(0xD5); oled_write_cmd(0x80);
    oled_write_cmd(0xA8); oled_write_cmd(0x3F);
    oled_write_cmd(0xD3); oled_write_cmd(0x00);
    oled_write_cmd(0x40);           /* start line 0 */
    oled_write_cmd(0xAD); oled_write_cmd(0x8B); /* SH1106 电荷泵开 (SSD1306 用 0x8D/0x14) */
    oled_write_cmd(0xA1);           /* 段重映射 */
    oled_write_cmd(0xC8);           /* COM 扫描反向 */
    oled_write_cmd(0xDA); oled_write_cmd(0x12);
    oled_write_cmd(0x81); oled_write_cmd(0xCF); /* 对比度 */
    oled_write_cmd(0xD9); oled_write_cmd(0xF1);
    oled_write_cmd(0xDB); oled_write_cmd(0x40);
    oled_write_cmd(0xA4);           /* 输出跟随显存 */
    oled_write_cmd(0xA6);           /* 正常显示 */
    oled_write_cmd(0xAF);           /* display on */

    OLED_Clear();
}

void OLED_ClearBuf(void)
{
    memset(s_gram, 0, sizeof(s_gram));
}

void OLED_Display(void)
{
    for (uint8_t page = 0; page < OLED_PAGES; page++) {
        oled_write_cmd(0xB0 + page);                       /* 页地址 */
        oled_write_cmd(0x00 + (OLED_COL_OFFSET & 0x0F));   /* 低列地址 */
        oled_write_cmd(0x10 + (OLED_COL_OFFSET >> 4));     /* 高列地址 */

        SoftI2C_Start();
        SoftI2C_WriteByte(OLED_I2C_ADDR);
        SoftI2C_WriteByte(0x40);                           /* 后续为数据 */
        for (uint8_t col = 0; col < OLED_WIDTH; col++) {
            SoftI2C_WriteByte(s_gram[page][col]);
        }
        SoftI2C_Stop();
    }
}

void OLED_Clear(void)
{
    OLED_ClearBuf();
    OLED_Display();
}

void OLED_DisplayOn(void)  { oled_write_cmd(0xAF); }
void OLED_DisplayOff(void) { oled_write_cmd(0xAE); }

/* 在显存绘制单个 6x8 字符 */
void OLED_ShowChar(uint8_t page, uint8_t col, char ch)
{
    if (page >= OLED_PAGES) return;
    if (ch < 0x20 || ch > 0x7E) ch = ' ';
    const uint8_t *glyph = F6x8[ch - 0x20];
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t c = col + i;
        if (c < OLED_WIDTH) s_gram[page][c] = glyph[i];
    }
}

void OLED_ShowString(uint8_t page, uint8_t col, const char *str)
{
    while (*str) {
        OLED_ShowChar(page, col, *str++);
        col += 6;
        if (col + 6 > OLED_WIDTH) { col = 0; page++; }
        if (page >= OLED_PAGES) break;
    }
}
