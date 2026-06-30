/**
 * @file    bsp_flash.c
 * @brief   内部 Flash 持久化实现（STM32F103C8T6，64KB Flash，页 1KB）
 *
 * 末页地址 = 0x08000000 + 63*1024 = 0x0800FC00。
 * STM32F1 仅支持半字(16bit)编程，这里按半字循环写入结构体。
 */
#include "bsp_flash.h"
#include "app_config.h"
#include <string.h>

#define FLASH_STORE_ADDR   ((uint32_t)0x0800FC00)   /* 末页起始 */
#define STORE_MAGIC        0x57415431u               /* "WAT1" */

uint8_t Flash_LoadSteps(StepStore_t *out)
{
    const StepStore_t *p = (const StepStore_t *)FLASH_STORE_ADDR;
    if (p->magic != STORE_MAGIC) {
        memset(out, 0, sizeof(StepStore_t));
        return 0;                       /* 未初始化 */
    }
    *out = *p;
    return 1;
}

uint8_t Flash_SaveSteps(const StepStore_t *in)
{
    StepStore_t buf = *in;
    buf.magic = STORE_MAGIC;

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_err = 0;
    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = FLASH_STORE_ADDR;
    erase.NbPages     = 1;
    if (HAL_FLASHEx_Erase(&erase, &page_err) != HAL_OK) {
        HAL_FLASH_Lock();
        return 1;
    }

    /* 按半字写入整个结构体 */
    const uint16_t *src = (const uint16_t *)&buf;
    uint32_t addr = FLASH_STORE_ADDR;
    uint32_t halfwords = (sizeof(StepStore_t) + 1) / 2;
    for (uint32_t i = 0; i < halfwords; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, src[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return 2;
        }
        addr += 2;
    }

    HAL_FLASH_Lock();
    return 0;
}
