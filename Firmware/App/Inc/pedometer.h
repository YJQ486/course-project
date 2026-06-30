/**
 * @file    pedometer.h
 * @brief   计步算法（合加速度 + 滑动平均 + 动态阈值波峰检测）
 *
 * 对齐《系统设计文档》关键算法描述：
 *   1) 合加速度 A = sqrt(ax^2+ay^2+az^2)
 *   2) 长度 N 滑动窗口算术平均，滤高频噪声
 *   3) 动态阈值 = 过去 50 点(max+min)/2，作为步态波峰基准线
 *   4) 上穿基准线 + 幅度足够 + 距上次步 >200ms 才计 1 步
 */
#ifndef PEDOMETER_H
#define PEDOMETER_H

#include <stdint.h>
#include "bsp_mpu6050.h"

void     Pedometer_Init(uint32_t total, uint32_t today);
/* 喂入一帧传感器数据；now_ms 为当前系统毫秒；返回本帧是否新增 1 步 */
uint8_t  Pedometer_Update(const MPU_Data_t *d, uint32_t now_ms);
uint32_t Pedometer_GetTotal(void);
uint32_t Pedometer_GetToday(void);
void     Pedometer_NewDay(void);     /* 跨天：当天步数清零，累计保留 */

#endif /* PEDOMETER_H */
