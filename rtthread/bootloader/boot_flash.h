#ifndef BOOT_FLASH_H__
#define BOOT_FLASH_H__

#include <stdint.h>
#include "ota_layout.h"

int boot_flash_write_range(uint32_t base, uint32_t offset, const uint8_t *data, uint32_t len);
int boot_flash_flush(void);
int boot_flash_copy_app(uint32_t image_size);
int boot_flash_meta_read(ota_meta_t *meta);
int boot_flash_meta_write(const ota_meta_t *meta);
int boot_app_valid(uint32_t *stack, uint32_t *entry);
void boot_flash_feed_wdt(void);

#endif /* BOOT_FLASH_H__ */
