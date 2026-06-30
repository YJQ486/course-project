/**
 * @file    bsp_encoder.h
 * @brief   EC11 旋转编码器驱动（TIM2 硬件正交解码 + 微动按键）
 */
#ifndef BSP_ENCODER_H
#define BSP_ENCODER_H

#include <stdint.h>

void    Encoder_Init(void);          /* 启动 TIM2 编码器模式 */

/* 返回自上次调用以来的有效“格”增量：顺时针为正、逆时针为负。
 * 已按 EC11 每格 4 计数做归一化。 */
int8_t  Encoder_GetDelta(void);

/* 微动按键是否按下（已消抖，按下沿返回 1，仅触发一次） */
uint8_t Encoder_KeyPressed(void);

#endif /* BSP_ENCODER_H */
