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
#include "app_config.h"     /* PED_DAILY_GOAL：计步页进度条目标 */
#include "app_tasks.h"      /* g_mpu_int_count：抬腕中断计数（标定用） */
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

/* ---------------- 通用 UI 组件 ---------------- */

/* 反白标题栏：顶部 11px 白条 + 黑色标题（左）+ 页码 n/N（右） */
static void ui_topbar(const char *title, uint8_t page)
{
    OLED_FillRect(0, 0, OLED_WIDTH, 11, 1);
    OLED_DrawStringEx(3, 2, title, 1, 1);
    char pi[6];
    snprintf(pi, sizeof(pi), "%u/%u", (unsigned)(page + 1), (unsigned)PAGE_COUNT);
    uint8_t w = OLED_StrWidth(pi, 1);
    OLED_DrawStringEx((uint8_t)(OLED_WIDTH - w - 3), 2, pi, 1, 1);
}

/* 进度条：外框 + 内部按 val/max 比例填充（h 建议 >=6） */
static void ui_progress(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                        uint32_t val, uint32_t max)
{
    OLED_DrawRect(x, y, w, h);
    if (max == 0 || w < 4 || h < 4) return;
    if (val > max) val = max;
    uint8_t inner = (uint8_t)(w - 4);
    uint8_t fill  = (uint8_t)((uint32_t)inner * val / max);
    if (fill) OLED_FillRect((uint8_t)(x + 2), (uint8_t)(y + 2), fill, (uint8_t)(h - 4), 1);
}

void Menu_Render(const UiData_t *ui)
{
    char line[24];
    OLED_ClearBuf();

    switch (s_page) {
    case PAGE_TIME: {
        ui_topbar("TIME", s_page);
        /* 大号 HH:MM（scale3=90px）+ 秒（scale2，底基线对齐），仿手表主界面 */
        char hm[8]; snprintf(hm, sizeof(hm), "%02d:%02d", ui->time.hour, ui->time.min);
        char ss[4]; snprintf(ss, sizeof(ss), "%02d", ui->time.sec);
        uint8_t hmw = OLED_StrWidth(hm, 3);
        uint8_t ssw = OLED_StrWidth(ss, 2);
        uint8_t x0  = (uint8_t)((OLED_WIDTH - (hmw + 4 + ssw)) / 2);
        OLED_DrawStringEx(x0, 16, hm, 3, 0);                        /* y16..39 */
        OLED_DrawStringEx((uint8_t)(x0 + hmw + 4), 24, ss, 2, 0);   /* 底对齐 */
        /* 秒进度条：每秒推进，直观证明 RTC 在走时 */
        ui_progress(9, 52, 110, 8, ui->time.sec, 59);
        break;
    }

    case PAGE_STEP: {
        ui_topbar("STEPS", s_page);
        char big[10]; snprintf(big, sizeof(big), "%lu", (unsigned long)ui->today_steps);
        uint8_t bw = OLED_StrWidth(big, 3);
        if (bw <= OLED_WIDTH) {
            OLED_DrawStringEx((uint8_t)((OLED_WIDTH - bw) / 2), 14, big, 3, 0);
        } else {                                  /* 位数过多则降一档避免越界 */
            bw = OLED_StrWidth(big, 2);
            OLED_DrawStringEx((uint8_t)((OLED_WIDTH - bw) / 2), 18, big, 2, 0);
        }
        ui_progress(9, 40, 110, 8, ui->today_steps, PED_DAILY_GOAL);
        snprintf(line, sizeof(line), "GOAL %u", (unsigned)PED_DAILY_GOAL);
        OLED_DrawStringEx(3, 52, line, 1, 0);
        snprintf(line, sizeof(line), "TOT %lu", (unsigned long)ui->total_steps);
        uint8_t w = OLED_StrWidth(line, 1);
        OLED_DrawStringEx((uint8_t)(OLED_WIDTH - w - 3), 52, line, 1, 0);
        break;
    }

    case PAGE_SENSOR: {
        /* 左列加速度 ACC、右列陀螺仪 GYRO（MPU6050 六轴，满足基本要求） */
        ui_topbar("SENSOR", s_page);
        OLED_DrawVLine(63, 13, 40);
        OLED_DrawStringEx(6,  14, "ACC",  1, 0);
        OLED_DrawStringEx(70, 14, "GYRO", 1, 0);
        snprintf(line, sizeof(line), "X%6d", ui->sensor.ax); OLED_DrawStringEx(2,  25, line, 1, 0);
        snprintf(line, sizeof(line), "Y%6d", ui->sensor.ay); OLED_DrawStringEx(2,  35, line, 1, 0);
        snprintf(line, sizeof(line), "Z%6d", ui->sensor.az); OLED_DrawStringEx(2,  45, line, 1, 0);
        snprintf(line, sizeof(line), "X%6d", ui->sensor.gx); OLED_DrawStringEx(66, 25, line, 1, 0);
        snprintf(line, sizeof(line), "Y%6d", ui->sensor.gy); OLED_DrawStringEx(66, 35, line, 1, 0);
        snprintf(line, sizeof(line), "Z%6d", ui->sensor.gz); OLED_DrawStringEx(66, 45, line, 1, 0);
        /* 运动中断计数：晃动模块该数应增长，是抬腕唤醒链路的自检/标定入口 */
        snprintf(line, sizeof(line), "INT:%lu", (unsigned long)g_mpu_int_count);
        OLED_DrawStringEx(2, 55, line, 1, 0);
        break;
    }

    case PAGE_SETTING: {
        ui_topbar("SET TIME", s_page);
        if (s_editing) {
            char t[12]; snprintf(t, sizeof(t), "%02d:%02d:%02d", s_eh, s_em, s_es);
            const uint8_t x0 = 16, y0 = 22;         /* scale2: 8 字符*12=96，居中 */
            OLED_DrawStringEx(x0, y0, t, 2, 0);
            /* 反白高亮当前编辑字段（时/分/秒 各 2 位） */
            uint8_t fx = (s_edit_field == 0) ? x0
                       : (s_edit_field == 1) ? (uint8_t)(x0 + 3 * 12)
                       :                       (uint8_t)(x0 + 6 * 12);
            uint8_t fv = (s_edit_field == 0) ? s_eh
                       : (s_edit_field == 1) ? s_em : s_es;
            OLED_FillRect((uint8_t)(fx - 1), (uint8_t)(y0 - 1), 25, 18, 1);
            char f2[4]; snprintf(f2, sizeof(f2), "%02d", fv);
            OLED_DrawStringEx(fx, y0, f2, 2, 1);    /* 白底黑字 */
            const char *tip = (s_edit_field == 0) ? "rotate=HOUR key=next" :
                              (s_edit_field == 1) ? "rotate=MIN  key=next" :
                                                    "rotate=SEC  key=save";
            OLED_DrawStringEx(3, 52, tip, 1, 0);
        } else {
            char t[12]; snprintf(t, sizeof(t), "%02d:%02d:%02d",
                                 ui->time.hour, ui->time.min, ui->time.sec);
            uint8_t w = OLED_StrWidth(t, 2);
            OLED_DrawStringEx((uint8_t)((OLED_WIDTH - w) / 2), 24, t, 2, 0);
            OLED_DrawStringEx(6, 50, "Press key to edit", 1, 0);
        }
        break;
    }

    default: break;
    }

    OLED_Display();
}
