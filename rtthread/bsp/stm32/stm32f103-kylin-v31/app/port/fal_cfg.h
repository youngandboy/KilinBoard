#ifndef FAL_CFG_H__
#define FAL_CFG_H__

#include <rtconfig.h>
#include <board.h>

/* Provided by bsp/stm32/libraries/HAL_Drivers/drivers/drv_flash/drv_flash_f1.c */
extern const struct fal_flash_dev stm32_onchip_flash;

#define FAL_FLASH_DEV_TABLE     \
{                               \
    &stm32_onchip_flash,        \
}

#ifdef FAL_PART_HAS_TABLE_CFG
#define FAL_PART_TABLE                                                              \
{                                                                                   \
    {FAL_PART_MAGIC_WORD, "app", "onchip_flash",          0, 480 * 1024, 0},        \
    {FAL_PART_MAGIC_WORD, "ef",  "onchip_flash", 480 * 1024,  32 * 1024, 0},        \
}
#endif

#endif /* FAL_CFG_H__ */
