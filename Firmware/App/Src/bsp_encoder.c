/**
 * @file    bsp_encoder.c
 * @brief   EC11 编码器驱动实现（TIM2_CH1/CH2 编码器模式，句柄 htim2）
 */
#include "bsp_encoder.h"
#include "app_config.h"

extern TIM_HandleTypeDef htim2;     /* CubeMX 生成，TIM2 配为 Encoder Mode */

#define ENC_COUNTS_PER_DETENT   4   /* EC11 常见每格 4 个正交计数 */

static uint16_t s_last_cnt = 0;
static uint8_t  s_key_last = 1;     /* 按键空闲为高（上拉） */

void Encoder_Init(void)
{
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    s_last_cnt = 0;
}

int8_t Encoder_GetDelta(void)
{
    uint16_t now = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
    int16_t diff = (int16_t)(now - s_last_cnt);   /* 利用无符号回绕求差 */

    /* 只在累计满一格时上报，避免抖动 */
    int8_t detents = (int8_t)(diff / ENC_COUNTS_PER_DETENT);
    if (detents != 0) {
        s_last_cnt += detents * ENC_COUNTS_PER_DETENT;
    }
    return detents;
}

uint8_t Encoder_KeyPressed(void)
{
    /* 简易消抖：检测下降沿（高->低），由调用方以 ~10ms 周期轮询 */
    uint8_t now = HAL_GPIO_ReadPin(ENC_KEY_PORT, ENC_KEY_PIN);
    uint8_t pressed = 0;
    if (s_key_last == 1 && now == 0) {
        pressed = 1;                /* 捕获按下沿 */
    }
    s_key_last = now;
    return pressed;
}
