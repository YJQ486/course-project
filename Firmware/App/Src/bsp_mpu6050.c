/**
 * @file    bsp_mpu6050.c
 * @brief   MPU6050 驱动实现（硬件 I2C1，句柄 hi2c1 由 CubeMX 生成）
 */
#include "bsp_mpu6050.h"
#include "app_config.h"

extern I2C_HandleTypeDef hi2c1;     /* CubeMX 生成 */

#define MPU_ADDR        (0x68 << 1) /* AD0=0 时地址 0x68，左移 1 位给 HAL */

/* 寄存器地址 */
#define REG_SMPLRT_DIV      0x19
#define REG_CONFIG          0x1A
#define REG_GYRO_CONFIG     0x1B
#define REG_ACCEL_CONFIG    0x1C
#define REG_MOT_THR         0x1F
#define REG_MOT_DUR         0x20
#define REG_INT_PIN_CFG     0x37
#define REG_INT_ENABLE      0x38
#define REG_ACCEL_XOUT_H    0x3B
#define REG_MOT_DETECT_CTRL 0x69
#define REG_PWR_MGMT_1      0x6B
#define REG_WHO_AM_I        0x75

static uint8_t mpu_write(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c1, MPU_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1, 100);
}

static uint8_t mpu_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, 100);
}

uint8_t MPU6050_Init(void)
{
    uint8_t id = 0;
    HAL_Delay(50);
    if (mpu_read(REG_WHO_AM_I, &id, 1) != HAL_OK) return 1;
    if (id != 0x68) return 2;               /* 设备识别失败 */

    if (mpu_write(REG_PWR_MGMT_1, 0x00) != HAL_OK) return 3; /* 解除休眠 */
    mpu_write(REG_SMPLRT_DIV, 0x07);        /* 采样分频 -> 1kHz/8 = 125Hz */
    mpu_write(REG_CONFIG, 0x06);            /* DLPF ~5Hz 低通，利于计步 */
    mpu_write(REG_GYRO_CONFIG, 0x18);       /* ±2000 dps */
    mpu_write(REG_ACCEL_CONFIG, 0x01);      /* ±2g, 高通 5Hz（bit2:0=001） */
    return 0;
}

uint8_t MPU6050_Read(MPU_Data_t *out)
{
    uint8_t b[14];
    if (mpu_read(REG_ACCEL_XOUT_H, b, 14) != HAL_OK) return 1;
    out->ax = (int16_t)((b[0]  << 8) | b[1]);
    out->ay = (int16_t)((b[2]  << 8) | b[3]);
    out->az = (int16_t)((b[4]  << 8) | b[5]);
    /* b[6],b[7] = 温度，跳过 */
    out->gx = (int16_t)((b[8]  << 8) | b[9]);
    out->gy = (int16_t)((b[10] << 8) | b[11]);
    out->gz = (int16_t)((b[12] << 8) | b[13]);
    return 0;
}

uint8_t MPU6050_EnableMotionInt(uint8_t thr, uint8_t dur)
{
    /* 参考 MPU6050 运动检测中断标准配置流程 */
    mpu_write(REG_PWR_MGMT_1, 0x00);
    mpu_write(REG_ACCEL_CONFIG, 0x01);      /* 加速度高通 5Hz */
    mpu_write(REG_MOT_DETECT_CTRL, 0x15);   /* 运动检测延时配置 */
    mpu_write(REG_MOT_THR, thr);            /* 运动阈值 */
    mpu_write(REG_MOT_DUR, dur);            /* 运动持续时间 */
    /* INT 推挽、高有效、读寄存器清中断；低功耗时由 EXTI 唤醒 */
    mpu_write(REG_INT_PIN_CFG, 0x20);
    mpu_write(REG_INT_ENABLE, 0x40);        /* 使能 Motion 中断 */
    return 0;
}
