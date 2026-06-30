/**
 * @file    menu.c
 * @brief   多级菜单状态机实现
 *
 * 顶层：编码器在 时间/计步/传感器/设置 四页间切换。
 * 设置页：按键进入“编辑”，编码器调当前字段，按键切到下一字段，
 *         走完 时->分->秒 后按键写回 RTC 并退出编辑。
 */
#include "menu.h"
#include "bsp_oled.h"
#include <stdio.h>

static MenuPage_t s_page;
static uint8_t    s_editing;        /* 是否处于校时编辑 */
static uint8_t    s_edit_field;     /* 0=时 1=分 2=秒 */
static uint8_t    s_eh, s_em, s_es; /* 编辑中的时分秒 */

void Menu_Init(void)
{
    s_page = PAGE_TIME;
    s_editing = 0;
    s_edit_field = 0;
}

MenuPage_t Menu_CurrentPage(void) { return s_page; }

static void clamp_field(void)
{
    if (s_edit_field == 0) s_eh %= 24;
    else                   { s_em %= 60; s_es %= 60; }
}

void Menu_Handle(int8_t delta, uint8_t key)
{
    if (s_page == PAGE_SETTING && s_editing) {
        /* 编辑模式：编码器调字段值 */
        if (delta != 0) {
            switch (s_edit_field) {
                case 0: s_eh = (uint8_t)((s_eh + 24 + delta) % 24); break;
                case 1: s_em = (uint8_t)((s_em + 60 + delta) % 60); break;
                case 2: s_es = (uint8_t)((s_es + 60 + delta) % 60); break;
            }
        }
        if (key) {
            s_edit_field++;
            if (s_edit_field > 2) {        /* 完成校时，写回 RTC */
                clamp_field();
                RTC_SetTime(s_eh, s_em, s_es);
                s_editing = 0;
                s_edit_field = 0;
            }
        }
        return;
    }

    /* 非编辑：编码器切页 */
    if (delta != 0) {
        int p = (int)s_page + delta;
        while (p < 0) p += PAGE_COUNT;
        s_page = (MenuPage_t)(p % PAGE_COUNT);
    }
    /* 在设置页按键 -> 进入编辑，以当前时间为初值 */
    if (key && s_page == PAGE_SETTING) {
        ClockTime_t now; RTC_GetTime(&now);
        s_eh = now.hour; s_em = now.min; s_es = now.sec;
        s_edit_field = 0;
        s_editing = 1;
    }
}

void Menu_Render(const UiData_t *ui)
{
    char line[24];
    OLED_ClearBuf();

    switch (s_page) {
    case PAGE_TIME:
        OLED_ShowString(0, 0, "== TIME ==");
        snprintf(line, sizeof(line), "%02d:%02d:%02d",
                 ui->time.hour, ui->time.min, ui->time.sec);
        OLED_ShowString(3, 32, line);
        break;

    case PAGE_STEP:
        OLED_ShowString(0, 0, "== STEPS ==");
        snprintf(line, sizeof(line), "Today: %lu", (unsigned long)ui->today_steps);
        OLED_ShowString(3, 0, line);
        snprintf(line, sizeof(line), "Total: %lu", (unsigned long)ui->total_steps);
        OLED_ShowString(5, 0, line);
        break;

    case PAGE_SENSOR:
        OLED_ShowString(0, 0, "== SENSOR ==");
        snprintf(line, sizeof(line), "AX:%6d", ui->sensor.ax);
        OLED_ShowString(2, 0, line);
        snprintf(line, sizeof(line), "AY:%6d", ui->sensor.ay);
        OLED_ShowString(4, 0, line);
        snprintf(line, sizeof(line), "AZ:%6d", ui->sensor.az);
        OLED_ShowString(6, 0, line);
        break;

    case PAGE_SETTING:
        OLED_ShowString(0, 0, "== SET TIME ==");
        if (s_editing) {
            snprintf(line, sizeof(line), ">%02d:%02d:%02d", s_eh, s_em, s_es);
            OLED_ShowString(3, 16, line);
            const char *tip = (s_edit_field == 0) ? "edit HOUR" :
                              (s_edit_field == 1) ? "edit MIN " : "edit SEC ";
            OLED_ShowString(6, 0, tip);
        } else {
            OLED_ShowString(3, 0, "Press key to edit");
        }
        break;

    default: break;
    }

    OLED_Display();
}
