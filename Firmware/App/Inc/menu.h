/**
 * @file    menu.h
 * @brief   多级菜单状态机（编码器翻页 + 按键确认；含校时子状态）
 */
#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "bsp_rtc.h"
#include "bsp_mpu6050.h"

typedef enum {
    PAGE_TIME = 0,    /* 时间页 */
    PAGE_STEP,        /* 计步页 */
    PAGE_SENSOR,      /* 传感器数据页（基本要求） */
    PAGE_SETTING,     /* 校时设置页 */
    PAGE_COUNT
} MenuPage_t;

typedef struct {
    ClockTime_t time;
    uint32_t    total_steps;
    uint32_t    today_steps;
    MPU_Data_t  sensor;
} UiData_t;

void        Menu_Init(void);
/* 处理输入：delta=编码器格增量，key=本帧是否按下 */
void        Menu_Handle(int8_t delta, uint8_t key);
/* 把当前页渲染到 OLED 显存并刷新 */
void        Menu_Render(const UiData_t *ui);
MenuPage_t  Menu_CurrentPage(void);

#endif /* MENU_H */
