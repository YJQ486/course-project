/**
 * @file    bsp_mpu6050.h
 * @brief   MPU6050 六轴传感器驱动（硬件 I2C1 + 运动/抬腕中断）
 */
#ifndef BSP_MPU6050_H
#define BSP_MPU6050_H

#include <stdint.h>

typedef struct {
    int16_t ax, ay, az;   /* 加速度原始值 (LSB) */
    int16_t gx, gy, gz;   /* 陀螺仪原始值 (LSB) */
} MPU_Data_t;

/* 返回 0 成功，非 0 失败（WHO_AM_I 校验不通过等） */
uint8_t MPU6050_Init(void);

/* 一次性读取加速度+陀螺仪原始数据 */
uint8_t MPU6050_Read(MPU_Data_t *out);

/* 配置运动检测中断（INT->PA4->EXTI），用于 STOP 模式抬腕唤醒。
 * thr: 运动阈值(LSB,越小越灵敏)，dur: 持续时间(ms 量级) */
uint8_t MPU6050_EnableMotionInt(uint8_t thr, uint8_t dur);

#endif /* BSP_MPU6050_H */
