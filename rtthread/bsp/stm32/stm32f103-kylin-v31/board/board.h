/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-5      SummerGift   first version
 */

#ifndef __BOARD_H__
#define __BOARD_H__

/* kyLin-V3.1 uses a 12 MHz HSE on PD0/PD1 (OSC_IN/OSC_OUT). */
#ifndef HSE_VALUE
#define HSE_VALUE 12000000U
#endif

#include <stm32f1xx.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STM32_FLASH_START_ADRESS     ((uint32_t)0x08000000)
#define STM32_FLASH_SIZE             (512 * 1024)
#define STM32_FLASH_END_ADDRESS      ((uint32_t)(STM32_FLASH_START_ADRESS + STM32_FLASH_SIZE))

/* OTA layout: bootloader 16K, app 232K, download 232K, EasyFlash 32K. */
#define APP_FLASH_BASE               ((uint32_t)0x08004000)
#define APP_FLASH_SIZE               (232U * 1024U)

/* Internal SRAM memory size[Kbytes] <8-64>, Default: 64*/
#define STM32_SRAM_SIZE      64
#define STM32_SRAM_END       (0x20000000 + STM32_SRAM_SIZE * 1024)

#if defined(__ARMCC_VERSION)
extern int Image$$RW_IRAM1$$ZI$$Limit;
#define HEAP_BEGIN      ((void *)&Image$$RW_IRAM1$$ZI$$Limit)
#elif __ICCARM__
#pragma section="CSTACK"
#define HEAP_BEGIN      (__segment_end("CSTACK"))
#else
extern int __bss_end;
#define HEAP_BEGIN      ((void *)&__bss_end)
#endif

#define HEAP_END        STM32_SRAM_END

void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_H__ */
