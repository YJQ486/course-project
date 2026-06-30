/**
 * @file    app_config.h
 * @brief   全局引脚分配与系统参数集中配置（与《系统设计文档》一致）
 *
 * 本文件把所有“与硬件相关 / 可调”的常量集中到一处，方便在面包板布线
 * 或 CubeMX 引脚变更时只改这一个文件。引脚命名必须与 CubeMX 中
 * User Label 一致（见 Firmware/CubeMX配置清单.md）。
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "main.h"   /* CubeMX 生成，内含 HAL 头与 GPIO 宏定义 */

/* ===================== 引脚分配（对齐设计文档硬件框图） ===================== */
/* OLED —— 软件模拟 I2C（规避 STM32F1 硬件 I2C 死锁缺陷） */
#define OLED_SCL_PORT       GPIOB
#define OLED_SCL_PIN        GPIO_PIN_10      /* PB10 */
#define OLED_SDA_PORT       GPIOB
#define OLED_SDA_PIN        GPIO_PIN_11      /* PB11 */

/* MPU6050 —— 硬件 I2C1（PB6=SCL / PB7=SDA），句柄 hi2c1 由 CubeMX 生成 */
/* MPU6050 运动/抬腕中断 INT -> PA4 -> EXTI4（STOP 唤醒源之一） */
#define MPU_INT_PORT        GPIOA
#define MPU_INT_PIN         GPIO_PIN_4

/* EC11 旋转编码器 A/B -> TIM2_CH1/CH2（PA0/PA1，编码器模式硬件正交解码） */
/* EC11 微动按键 -> PB12 -> EXTI（STOP 唤醒源之一、菜单确认键） */
#define ENC_KEY_PORT        GPIOB
#define ENC_KEY_PIN         GPIO_PIN_12

/* 一键开机：上电后主控拉高维持 GPIO，保持 NPN 导通实现软自锁 */
#define PWR_HOLD_PORT       GPIOA
#define PWR_HOLD_PIN        GPIO_PIN_8

/* JDY-31 经典蓝牙 -> USART2（PA2=TX / PA3=RX），句柄 huart2 由 CubeMX 生成 */

/* ===================== 计步算法参数 ===================== */
#define PED_SAMPLE_MS           20      /* 采样周期 20ms => 50Hz */
#define PED_WINDOW_SIZE         8       /* 滑动平均窗口长度 N */
#define PED_DYN_WINDOW          50      /* 动态阈值统计窗口（过去 50 点） */
#define PED_MIN_STEP_INTERVAL   200     /* 相邻有效步最小时间间隔(ms)，防抖动多计 */
#define PED_MIN_AMPLITUDE       1500    /* 波峰最小幅度门限（原始 LSB），过滤静止噪声 */

/* ===================== 低功耗参数 ===================== */
#define PWR_IDLE_TIMEOUT_MS     10000   /* 无操作 10s 进入 STOP */
#define PWR_CHECK_PERIOD_MS     500     /* Task_Power 周期唤醒间隔（非忙等轮询） */

/* ===================== 任务优先级（原生 FreeRTOS 数值，越大越高） ===================== */
/* 与文档对应：Power=最高(4) Sensor=高(3) UI=中(2) Comm=低(1) */
#define PRIO_POWER          (tskIDLE_PRIORITY + 4)
#define PRIO_SENSOR         (tskIDLE_PRIORITY + 3)
#define PRIO_UI             (tskIDLE_PRIORITY + 2)
#define PRIO_COMM           (tskIDLE_PRIORITY + 1)

/* 任务栈深度（单位：字 word，非字节） */
#define STK_POWER           128
#define STK_SENSOR          256
#define STK_UI              256
#define STK_COMM            256

#endif /* APP_CONFIG_H */
