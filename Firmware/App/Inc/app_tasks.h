/**
 * @file    app_tasks.h
 * @brief   应用入口：FreeRTOS 任务创建与系统集成
 *
 * 集成方式（详见 Firmware/集成说明.md）：
 *   在 main() 中 MX_xxx_Init() 全部完成后、启动调度器前调用：
 *       App_Init();          // 初始化各 BSP 驱动、载入 Flash 步数
 *       App_CreateTasks();   // 创建互斥锁/信号量与四大任务
 *   然后由 vTaskStartScheduler()（或 CubeMX 的 osKernelStart）启动调度。
 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdint.h>

void App_Init(void);
void App_CreateTasks(void);

/* MPU 运动中断累计次数（SENSOR 页显示，用于抬腕阈值现场标定） */
extern volatile uint32_t g_mpu_int_count;

#endif /* APP_TASKS_H */
