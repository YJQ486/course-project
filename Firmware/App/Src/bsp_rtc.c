/**
 * @file    bsp_rtc.c
 * @brief   片内 RTC 走时实现（句柄 hrtc 由 CubeMX 生成，时钟源选 LSE）
 *
 * STM32F1 的 RTC 为 32 位秒计数器，HAL 在其上用 Backup 寄存器维护“日期”。
 * 关键点：HAL_RTC_GetTime 后必须紧跟 HAL_RTC_GetDate 以解锁 shadow 寄存器，
 * 否则计数器会被锁住不再更新。
 */
#include "bsp_rtc.h"
#include "app_config.h"

extern RTC_HandleTypeDef hrtc;      /* CubeMX 生成 */

void RTC_SetTime(uint8_t h, uint8_t m, uint8_t s)
{
    RTC_TimeTypeDef t = {0};
    t.Hours = h; t.Minutes = m; t.Seconds = s;
    HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
}

void RTC_GetTime(ClockTime_t *out)
{
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);   /* 必须连读以解锁 */
    out->hour = t.Hours;
    out->min  = t.Minutes;
    out->sec  = t.Seconds;
}

uint16_t RTC_GetDayIndex(void)
{
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    /* 简化的天序号：年*372 + 月*31 + 日，足以做“跨天”判定 */
    return (uint16_t)d.Year * 372 + (uint16_t)d.Month * 31 + d.Date;
}
