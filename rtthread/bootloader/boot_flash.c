#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_iwdg.h"
#include "boot_flash.h"

static uint8_t page_buf[OTA_FLASH_PAGE_SIZE];
static uint32_t page_addr;
static uint32_t page_used;
static int page_valid;

void boot_flash_feed_wdt(void)
{
    IWDG_ReloadCounter();
}

static void flash_clear_flags(void)
{
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
}

static int flash_erase_and_program_page(uint32_t addr, const uint8_t *data)
{
    uint32_t i = 0;
    int result = 0;

    FLASH_Unlock();
    flash_clear_flags();

    if (FLASH_ErasePage(addr) != FLASH_COMPLETE)
    {
        result = -1;
        goto out;
    }

    for (i = 0; i < OTA_FLASH_PAGE_SIZE; i += 2U)
    {
        uint16_t half = (uint16_t)data[i] | ((uint16_t)data[i + 1U] << 8);
        if (FLASH_ProgramHalfWord(addr + i, half) != FLASH_COMPLETE)
        {
            result = -1;
            break;
        }
        if ((i & 0x3FFU) == 0U)
        {
            boot_flash_feed_wdt();
        }
    }

out:
    FLASH_Lock();
    return result;
}

int boot_flash_flush(void)
{
    int result = 0;

    if (!page_valid)
    {
        return 0;
    }

    result = flash_erase_and_program_page(page_addr, page_buf);
    page_valid = 0;
    page_used = 0;
    return result;
}

int boot_flash_write_range(uint32_t base, uint32_t offset, const uint8_t *data, uint32_t len)
{
    uint32_t index = 0;

    if (data == 0)
    {
        return -1;
    }
    if ((base == OTA_DOWNLOAD_BASE) && ((offset + len) > OTA_DOWNLOAD_SIZE))
    {
        return -1;
    }
    if ((base == OTA_APP_BASE) && ((offset + len) > OTA_APP_SIZE))
    {
        return -1;
    }

    while (index < len)
    {
        uint32_t absolute = base + offset + index;
        uint32_t current_page = absolute & ~(OTA_FLASH_PAGE_SIZE - 1U);
        uint32_t in_page = absolute - current_page;

        if (!page_valid || (current_page != page_addr))
        {
            if (boot_flash_flush() != 0)
            {
                return -1;
            }

            page_addr = current_page;
            page_used = in_page;
            page_valid = 1;

            /* Start with erased state; callers write pages sequentially. */
            for (uint32_t i = 0; i < OTA_FLASH_PAGE_SIZE; i++)
            {
                page_buf[i] = 0xFFU;
            }
        }

        while ((index < len) && (page_used < OTA_FLASH_PAGE_SIZE))
        {
            page_buf[page_used++] = data[index++];
        }

        if (page_used >= OTA_FLASH_PAGE_SIZE)
        {
            if (boot_flash_flush() != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

int boot_flash_copy_app(uint32_t image_size)
{
    const uint8_t *src = (const uint8_t *)(OTA_DOWNLOAD_BASE + sizeof(ota_header_t));

    if (image_size > OTA_APP_SIZE)
    {
        return -1;
    }

    if (boot_flash_flush() != 0)
    {
        return -1;
    }

    if (boot_flash_write_range(OTA_APP_BASE, 0, src, image_size) != 0)
    {
        return -1;
    }

    return boot_flash_flush();
}

int boot_flash_meta_read(ota_meta_t *meta)
{
    const ota_meta_t *stored = (const ota_meta_t *)OTA_META_BASE;

    for (uint32_t i = 0; i < sizeof(*meta); i++)
    {
        ((uint8_t *)meta)[i] = ((const uint8_t *)stored)[i];
    }

    if (meta->magic != OTA_META_MAGIC)
    {
        meta->magic = OTA_META_MAGIC;
        meta->state = OTA_META_STATE_IDLE;
        meta->image_size = 0;
        meta->image_crc32 = 0;
        meta->version = 0;
    }
    return 0;
}

int boot_flash_meta_write(const ota_meta_t *meta)
{
    const uint8_t *data = (const uint8_t *)meta;
    uint32_t i;

    FLASH_Unlock();
    flash_clear_flags();

    if (FLASH_ErasePage(OTA_META_BASE) != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return -1;
    }

    for (i = 0; i < sizeof(*meta); i += 2U)
    {
        uint16_t half = (uint16_t)data[i] | ((uint16_t)data[i + 1U] << 8);
        if (FLASH_ProgramHalfWord(OTA_META_BASE + i, half) != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return -1;
        }
    }

    FLASH_Lock();
    return 0;
}

int boot_app_valid(uint32_t *stack, uint32_t *entry)
{
    uint32_t sp = *(volatile uint32_t *)OTA_APP_BASE;
    uint32_t pc = *(volatile uint32_t *)(OTA_APP_BASE + 4U);

    *stack = sp;
    *entry = pc;

    if ((sp < 0x20000000U) || (sp > 0x20010000U))
    {
        return 0;
    }
    if ((pc < OTA_APP_BASE) || (pc >= (OTA_APP_BASE + OTA_APP_SIZE)))
    {
        return 0;
    }
    return 1;
}
