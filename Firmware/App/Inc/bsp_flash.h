/**
 * @file    bsp_flash.h
 * @brief   内部 Flash 步数历史持久化（掉电保存）
 *
 * 在片内 Flash 末页存储累计步数与当天步数，实现关机/掉电后步数不丢。
 * 采用“低频写入 + 关机前落盘”策略，降低擦写磨损。
 */
#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include <stdint.h>

typedef struct {
    uint32_t magic;        /* 校验魔数，判断是否已初始化 */
    uint32_t total_steps;  /* 历史累计步数 */
    uint32_t today_steps;  /* 当天步数 */
    uint16_t day_index;    /* 写入时的天序号，用于跨天清零 */
    uint16_t reserved;
} StepStore_t;

/* 读取存储；返回 1=有效数据已载入，0=Flash 为空（首次使用） */
uint8_t Flash_LoadSteps(StepStore_t *out);

/* 写入存储（先擦末页再写）。返回 0 成功 */
uint8_t Flash_SaveSteps(const StepStore_t *in);

#endif /* BSP_FLASH_H */
