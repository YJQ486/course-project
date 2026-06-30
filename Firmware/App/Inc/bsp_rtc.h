/**
 * @file    bsp_rtc.h
 * @brief   片内 RTC 走时封装（LSE 32.768kHz，STOP 模式下持续走时）
 */
#ifndef BSP_RTC_H
#define BSP_RTC_H

#include <stdint.h>

typedef struct {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} ClockTime_t;

void RTC_SetTime(uint8_t h, uint8_t m, uint8_t s);
void RTC_GetTime(ClockTime_t *t);

/* 返回“当天序号”，用于计步按天清零（基于 RTC 日期，跨天检测） */
uint16_t RTC_GetDayIndex(void);

#endif /* BSP_RTC_H */
