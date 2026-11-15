/* Minimal OTA bootloader for kyLin-V3.1.
 *
 * Flash layout is defined in ../ota_layout.h. The bootloader is never
 * updated by OTA; it receives a container through Ymodem, validates it and
 * copies the payload to the application slot.
 */

#include "stm32f10x.h"
#include "boot_flash.h"
#include "boot_time.h"
#include "boot_uart.h"
#include "boot_ymodem.h"
#include "ota_layout.h"

#define BOOT_LED_PORT   GPIOB
#define BOOT_LED_PIN    GPIO_Pin_6

static void boot_delay(volatile uint32_t count)
{
    while (count-- != 0U)
    {
    }
}

static void boot_led_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = BOOT_LED_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(BOOT_LED_PORT, &gpio);
    GPIO_SetBits(BOOT_LED_PORT, BOOT_LED_PIN); /* LED off (active low). */
}

static void boot_led_blink(void)
{
    GPIO_ResetBits(BOOT_LED_PORT, BOOT_LED_PIN);
    boot_delay(150000U);
    GPIO_SetBits(BOOT_LED_PORT, BOOT_LED_PIN);
    boot_delay(150000U);
}

static void boot_print_hex32(uint32_t value)
{
    static const char hex[] = "0123456789abcdef";
    int shift;

    boot_uart_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4)
    {
        boot_uart_putc(hex[(value >> (uint32_t)shift) & 0xFU]);
    }
}

static int boot_key_pressed(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    gpio.GPIO_Pin = GPIO_Pin_13;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &gpio);

    return (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == Bit_RESET) ? 1 : 0;
}

static uint32_t boot_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t i;

    while (len-- != 0U)
    {
        crc ^= *data++;
        for (i = 0; i < 8U; i++)
        {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
        }
    }
    return ~crc;
}

static int boot_validate_download(uint32_t file_size,
                                  uint32_t *image_size,
                                  uint32_t *image_crc,
                                  uint32_t *version)
{
    const ota_header_t *header = (const ota_header_t *)OTA_DOWNLOAD_BASE;
    uint8_t i;

    if (file_size < sizeof(ota_header_t))
    {
        boot_uart_puts("[boot] file too small\r\n");
        return -1;
    }

    boot_uart_puts("[boot] image file=");
    boot_print_hex32(file_size);
    boot_uart_puts(" size=");
    boot_print_hex32(header->image_size);
    boot_uart_puts(" crc=");
    boot_print_hex32(header->image_crc32);
    boot_uart_puts("\r\n");

    for (i = 0; i < OTA_HEADER_MAGIC_SIZE; i++)
    {
        if (header->magic[i] != ota_header_magic[i])
        {
            boot_uart_puts("[boot] bad magic\r\n");
            return -1;
        }
    }

    if ((header->image_size == 0U) || (header->image_size > OTA_APP_SIZE))
    {
        boot_uart_puts("[boot] bad image size\r\n");
        return -1;
    }
    if (file_size < (sizeof(ota_header_t) + header->image_size))
    {
        boot_uart_puts("[boot] truncated image\r\n");
        return -1;
    }

    if (boot_crc32((const uint8_t *)header, 20U) != header->header_crc32)
    {
        return -1;
    }
    if (boot_crc32((const uint8_t *)header + sizeof(ota_header_t),
                   header->image_size) != header->image_crc32)
    {
        return -1;
    }

    *image_size = header->image_size;
    *image_crc = header->image_crc32;
    *version = header->version;
    return 0;
}

static int boot_apply_download(uint32_t image_size, uint32_t image_crc, uint32_t version)
{
    ota_meta_t meta;

    boot_flash_meta_read(&meta);
    meta.magic = OTA_META_MAGIC;
    meta.state = OTA_META_STATE_DOWNLOAD;
    meta.image_size = image_size;
    meta.image_crc32 = image_crc;
    meta.version = version;

    if (boot_flash_meta_write(&meta) != 0)
    {
        boot_uart_puts("[boot] meta write failed\r\n");
        return -1;
    }

    boot_uart_puts("[boot] copying to app...\r\n");
    if (boot_flash_copy_app(image_size) != 0)
    {
        boot_uart_puts("[boot] copy failed\r\n");
        return -1;
    }

    if (boot_crc32((const uint8_t *)OTA_APP_BASE, image_size) != image_crc)
    {
        boot_uart_puts("[boot] app CRC failed\r\n");
        return -1;
    }

    meta.state = OTA_META_STATE_IDLE;
    if (boot_flash_meta_write(&meta) != 0)
    {
        boot_uart_puts("[boot] meta clear failed\r\n");
        return -1;
    }

    boot_uart_puts("[boot] update OK, version=");
    boot_print_hex32(version);
    boot_uart_puts("\r\n");
    return 0;
}

static void boot_jump(uint32_t stack, uint32_t entry)
{
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    USART_Cmd(USART1, DISABLE);
    SCB->VTOR = OTA_APP_BASE;
    __set_MSP(stack);
    ((void (*)(void))entry)();

    while (1)
    {
    }
}

void __libc_init_array(void)
{
}

void *memcpy(void *dst, const void *src, unsigned int len)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    while (len-- != 0U)
    {
        *d++ = *s++;
    }
    return dst;
}

void *memset(void *dst, int value, unsigned int len)
{
    unsigned char *d = (unsigned char *)dst;

    while (len-- != 0U)
    {
        *d++ = (unsigned char)value;
    }
    return dst;
}

int main(void)
{
    ota_meta_t meta;
    uint32_t stack = 0;
    uint32_t entry = 0;
    uint32_t image_size = 0;
    uint32_t image_crc = 0;
    uint32_t version = 0;
    int app_ok;
    int force_update;

    boot_time_init();
    boot_uart_init();
    boot_led_init();

    boot_uart_puts("\r\n[boot] kyLin-V3.1 OTA bootloader\r\n");

    boot_flash_meta_read(&meta);
    app_ok = boot_app_valid(&stack, &entry);
    force_update = boot_key_pressed();

    boot_uart_puts("[boot] app=");
    boot_uart_puts(app_ok ? "valid" : "invalid");
    boot_uart_puts(" state=");
    boot_print_hex32(meta.state);
    boot_uart_puts(" key=");
    boot_uart_puts(force_update ? "down" : "up");
    boot_uart_puts("\r\n");

    if ((meta.state == OTA_META_STATE_DOWNLOAD) && !force_update)
    {
        if ((boot_validate_download(OTA_APP_SIZE, &image_size, &image_crc, &version) == 0) &&
            (boot_apply_download(image_size, image_crc, version) == 0))
        {
            app_ok = boot_app_valid(&stack, &entry);
            if (app_ok)
            {
                boot_jump(stack, entry);
            }
        }
        else
        {
            boot_uart_puts("[boot] stored download failed\r\n");
        }

        /* A failed stored update must not leave the board in a boot loop. */
        meta.state = OTA_META_STATE_IDLE;
        boot_flash_meta_write(&meta);
        app_ok = boot_app_valid(&stack, &entry);
        if (app_ok)
        {
            boot_uart_puts("[boot] booting old app\r\n");
            boot_jump(stack, entry);
        }
    }

    if (force_update || (meta.state == OTA_META_STATE_REQUEST) || !app_ok)
    {
        uint32_t file_size = 0;

        boot_uart_puts("[boot] waiting for Ymodem (115200 8N1)\r\n");
        if (boot_ymodem_receive(&file_size) == 0)
        {
            if (boot_validate_download(file_size, &image_size, &image_crc, &version) == 0)
            {
                if (boot_apply_download(image_size, image_crc, version) == 0)
                {
                    app_ok = boot_app_valid(&stack, &entry);
                    if (app_ok)
                    {
                        boot_jump(stack, entry);
                    }
                }
            }
            else
            {
                boot_uart_puts("[boot] image validation failed\r\n");
            }
        }
        else
        {
            boot_uart_puts("[boot] Ymodem receive failed\r\n");
        }

        meta.state = OTA_META_STATE_IDLE;
        boot_flash_meta_write(&meta);

        app_ok = boot_app_valid(&stack, &entry);
        if (app_ok)
        {
            boot_uart_puts("[boot] booting old app\r\n");
            boot_jump(stack, entry);
        }
    }

    app_ok = boot_app_valid(&stack, &entry);
    if (app_ok)
    {
        boot_uart_puts("[boot] booting app\r\n");
        boot_jump(stack, entry);
    }

    boot_uart_puts("[boot] no valid app, stay in bootloader\r\n");
    while (1)
    {
        boot_led_blink();
    }
}
