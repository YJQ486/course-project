/**
 * @file    pedometer.c
 * @brief   计步算法实现（纯整数运算，适合 Cortex-M3 无 FPU）
 */
#include "pedometer.h"
#include "app_config.h"

static uint32_t s_total;
static uint32_t s_today;

/* 滑动平均窗口 */
static uint32_t s_avg_buf[PED_WINDOW_SIZE];
static uint8_t  s_avg_idx;
static uint8_t  s_avg_filled;

/* 动态阈值统计窗口（环形缓冲存平滑后的合加速度） */
static uint16_t s_dyn_buf[PED_DYN_WINDOW];
static uint8_t  s_dyn_idx;
static uint8_t  s_dyn_filled;

static uint8_t  s_above;            /* 上一次是否在阈值之上 */
static uint32_t s_last_step_ms;     /* 上次有效计步时间 */

/* 整数平方根（uint32 -> uint16） */
static uint16_t isqrt32(uint32_t x)
{
    uint32_t res = 0;
    uint32_t bit = 1UL << 30;
    while (bit > x) bit >>= 2;
    while (bit) {
        if (x >= res + bit) { x -= res + bit; res = (res >> 1) + bit; }
        else                {                 res >>= 1; }
        bit >>= 2;
    }
    return (uint16_t)res;
}

void Pedometer_Init(uint32_t total, uint32_t today)
{
    s_total = total;
    s_today = today;
    s_avg_idx = s_avg_filled = 0;
    s_dyn_idx = s_dyn_filled = 0;
    s_above = 0;
    s_last_step_ms = 0;
}

uint32_t Pedometer_GetTotal(void) { return s_total; }
uint32_t Pedometer_GetToday(void) { return s_today; }
void     Pedometer_NewDay(void)   { s_today = 0; }

uint8_t Pedometer_Update(const MPU_Data_t *d, uint32_t now_ms)
{
    /* 1) 合加速度（uint32 防溢出：满量程 3*32767^2 ≈ 3.2e9 < 4.29e9） */
    uint32_t sq = (uint32_t)((int32_t)d->ax * d->ax)
                + (uint32_t)((int32_t)d->ay * d->ay)
                + (uint32_t)((int32_t)d->az * d->az);
    uint16_t mag = isqrt32(sq);

    /* 2) 滑动平均平滑 */
    s_avg_buf[s_avg_idx] = mag;
    s_avg_idx = (s_avg_idx + 1) % PED_WINDOW_SIZE;
    if (s_avg_idx == 0) s_avg_filled = 1;
    uint8_t avg_n = s_avg_filled ? PED_WINDOW_SIZE : s_avg_idx;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < avg_n; i++) sum += s_avg_buf[i];
    uint16_t smooth = (uint16_t)(sum / avg_n);

    /* 3) 动态阈值：过去 PED_DYN_WINDOW 点的 (max+min)/2 */
    s_dyn_buf[s_dyn_idx] = smooth;
    s_dyn_idx = (s_dyn_idx + 1) % PED_DYN_WINDOW;
    if (s_dyn_idx == 0) s_dyn_filled = 1;
    uint8_t dyn_n = s_dyn_filled ? PED_DYN_WINDOW : s_dyn_idx;
    uint16_t vmax = 0, vmin = 0xFFFF;
    for (uint8_t i = 0; i < dyn_n; i++) {
        uint16_t v = s_dyn_buf[i];
        if (v > vmax) vmax = v;
        if (v < vmin) vmin = v;
    }
    uint16_t threshold = (uint16_t)(((uint32_t)vmax + vmin) / 2);
    uint16_t amplitude = vmax - vmin;

    /* 4) 上穿阈值 + 幅度足够 + 时间间隔达标 => 计 1 步 */
    uint8_t step = 0;
    uint8_t now_above = (smooth > threshold) ? 1 : 0;
    if (now_above && !s_above) {                     /* 上升沿穿越 */
        if (amplitude > PED_MIN_AMPLITUDE &&
            (now_ms - s_last_step_ms) > PED_MIN_STEP_INTERVAL) {
            s_total++;
            s_today++;
            s_last_step_ms = now_ms;
            step = 1;
        }
    }
    s_above = now_above;
    return step;
}
