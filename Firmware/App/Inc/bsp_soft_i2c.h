/**
 * @file    bsp_soft_i2c.h
 * @brief   OLED 专用软件模拟 I2C（GPIO 位翻转，开漏）
 *
 * 仅用于 OLED 单向写，故只实现 Start/Stop/写字节/等 ACK。
 * 引脚与时序参数见 app_config.h。
 */
#ifndef BSP_SOFT_I2C_H
#define BSP_SOFT_I2C_H

#include <stdint.h>

void SoftI2C_Init(void);                 /* 初始化前请确保 GPIO 时钟已开 */
void SoftI2C_Start(void);
void SoftI2C_Stop(void);
uint8_t SoftI2C_WriteByte(uint8_t data); /* 返回 0=收到 ACK, 1=NACK */

#endif /* BSP_SOFT_I2C_H */
