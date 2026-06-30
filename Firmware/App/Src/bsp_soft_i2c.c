/**
 * @file    bsp_soft_i2c.c
 * @brief   OLED 专用软件模拟 I2C 实现（开漏，标准模式 ~100-400kHz）
 */
#include "bsp_soft_i2c.h"
#include "app_config.h"

/* 简易微秒级延时：72MHz 下约 1 循环=若干 ns，此处用空操作粗调时序。
 * 若需要更精确可改用 DWT 计数。OLED 对速率不敏感，粗延时即可稳定工作。 */
static inline void i2c_delay(void)
{
    for (volatile uint32_t i = 0; i < 8; i++) { __NOP(); }
}

/* SCL/SDA 开漏控制：输出 1 = 释放（外部上拉拉高），输出 0 = 拉低 */
#define SCL_H()  HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET)
#define SCL_L()  HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_RESET)
#define SDA_H()  HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET)
#define SDA_L()  HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_RESET)
#define SDA_READ() HAL_GPIO_ReadPin(OLED_SDA_PORT, OLED_SDA_PIN)

/* 将 SCL/SDA 配置为开漏输出。CubeMX 中也可直接把这两脚设为 GPIO_Output
 * Open-Drain + Pull-Up；此处再保证一次，避免误配。 */
void SoftI2C_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = OLED_SCL_PIN;
    HAL_GPIO_Init(OLED_SCL_PORT, &gpio);
    gpio.Pin = OLED_SDA_PIN;
    HAL_GPIO_Init(OLED_SDA_PORT, &gpio);

    SCL_H();
    SDA_H();
}

void SoftI2C_Start(void)
{
    SDA_H(); SCL_H(); i2c_delay();
    SDA_L(); i2c_delay();       /* SCL 高时 SDA 下降沿 = Start */
    SCL_L(); i2c_delay();
}

void SoftI2C_Stop(void)
{
    SDA_L(); SCL_H(); i2c_delay();
    SDA_H(); i2c_delay();       /* SCL 高时 SDA 上升沿 = Stop */
}

uint8_t SoftI2C_WriteByte(uint8_t data)
{
    uint8_t ack;
    for (uint8_t i = 0; i < 8; i++) {
        if (data & 0x80) SDA_H(); else SDA_L();
        data <<= 1;
        i2c_delay();
        SCL_H(); i2c_delay();
        SCL_L(); i2c_delay();
    }
    /* 第 9 个时钟读 ACK */
    SDA_H();                    /* 释放 SDA 由从机拉低 */
    i2c_delay();
    SCL_H(); i2c_delay();
    ack = (SDA_READ() == GPIO_PIN_RESET) ? 0 : 1;
    SCL_L(); i2c_delay();
    return ack;
}
