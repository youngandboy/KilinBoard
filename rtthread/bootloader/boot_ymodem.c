#include "boot_flash.h"
#include "boot_time.h"
#include "boot_uart.h"
#include "boot_ymodem.h"
#include "ota_layout.h"

#define YM_SOH      0x01U
#define YM_STX      0x02U
#define YM_EOT      0x04U
#define YM_ACK      0x06U
#define YM_NAK      0x15U
#define YM_CAN      0x18U
#define YM_C        0x43U

#define YM_PACKET_128   128U
#define YM_PACKET_1K    1024U
#define YM_UNKNOWN_SIZE 0xFFFFFFFFU

static uint16_t ym_crc16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0;
    uint32_t i;

    while (len-- != 0U)
    {
        crc ^= (uint16_t)(*data++) << 8;
        for (i = 0; i < 8U; i++)
        {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static uint32_t ym_parse_size(const uint8_t *packet, uint32_t packet_len)
{
    uint32_t name_len = 0;
    uint32_t value = 0;
    const uint8_t *p;

    while ((name_len < packet_len) && (packet[name_len] != 0U))
    {
        name_len++;
    }
    if ((name_len + 1U) >= packet_len)
    {
        return 0;
    }

    p = packet + name_len + 1U;
    while ((p < packet + packet_len) && (*p >= '0') && (*p <= '9'))
    {
        value = (value * 10U) + (uint32_t)(*p - '0');
        p++;
    }
    return value;
}

/* Returns 0 on timeout, 1 on packet, 2 on EOT, -1 on CAN/error. */
static int ym_read_packet(uint8_t *data, uint32_t *data_len, uint8_t *seq)
{
    int ch;
    int nseq;
    uint32_t size;
    uint32_t i;
    uint16_t got_crc;
    uint16_t calc_crc;

    ch = boot_uart_getc(200);
    if (ch < 0)
    {
        return 0;
    }
    if (ch == (int)YM_EOT)
    {
        return 2;
    }
    if (ch == (int)YM_CAN)
    {
        return -1;
    }
    if ((ch != (int)YM_SOH) && (ch != (int)YM_STX))
    {
        return -3;
    }

    size = (ch == (int)YM_STX) ? YM_PACKET_1K : YM_PACKET_128;

    ch = boot_uart_getc(1000);
    if (ch < 0)
    {
        return 0;
    }
    *seq = (uint8_t)ch;

    nseq = boot_uart_getc(1000);
    if (nseq < 0)
    {
        return 0;
    }
    if ((uint8_t)nseq != (uint8_t)(~(*seq)))
    {
        boot_uart_putc((char)YM_NAK);
        return -3;
    }

    for (i = 0; i < size; i++)
    {
        ch = boot_uart_getc(1000);
        if (ch < 0)
        {
            boot_uart_putc((char)YM_NAK);
            return 0;
        }
        data[i] = (uint8_t)ch;
    }

    ch = boot_uart_getc(1000);
    if (ch < 0)
    {
        boot_uart_putc((char)YM_NAK);
        return 0;
    }
    got_crc = (uint16_t)ch << 8;
    ch = boot_uart_getc(1000);
    if (ch < 0)
    {
        boot_uart_putc((char)YM_NAK);
        return 0;
    }
    got_crc |= (uint16_t)ch;

    calc_crc = ym_crc16(data, size);
    if (got_crc != calc_crc)
    {
        boot_uart_putc((char)YM_NAK);
        return -3;
    }

    *data_len = size;
    return 1;
}

int boot_ymodem_receive(uint32_t *file_size)
{
    uint8_t packet[YM_PACKET_1K];
    uint8_t packet_seq = 0;
    uint32_t packet_len = 0;
    uint32_t expected_seq = 0;
    uint32_t expected_size = YM_UNKNOWN_SIZE;
    uint32_t total = 0;
    uint32_t start = boot_millis();
    uint32_t last_rx = boot_millis();
    uint32_t last_c = 0;
    int started = 0;
    int finishing = 0;
    int phase;

    while (1)
    {
        boot_flash_feed_wdt();

        phase = ym_read_packet(packet, &packet_len, &packet_seq);
        if (phase == 2)
        {
            boot_uart_putc((char)YM_ACK);
            boot_uart_putc((char)YM_C);
            last_c = boot_millis();
            finishing = 1;
            expected_seq = 0;
            continue;
        }
        if (phase == -1)
        {
            return -1;
        }
        if (phase < 0)
        {
            continue;
        }
        if (phase == 0)
        {
            if (!started)
            {
                if ((boot_millis() - start) >= 60000U)
                {
                    return -1;
                }
                if ((boot_millis() - last_c) >= 1000U)
                {
                    boot_uart_putc((char)YM_C);
                    last_c = boot_millis();
                }
            }
            if (started && ((boot_millis() - last_rx) >= 30000U))
            {
                return -1;
            }
            continue;
        }

        last_rx = boot_millis();

        if (finishing)
        {
            /* Final empty header after EOT. */
            boot_uart_putc((char)YM_ACK);
            if (boot_flash_flush() != 0)
            {
                return -1;
            }
            *file_size = total;
            return 0;
        }

        if (!started)
        {
            if (packet[0] == 0U)
            {
                boot_uart_putc((char)YM_ACK);
                boot_uart_putc((char)YM_C);
                continue;
            }

            expected_size = ym_parse_size(packet, packet_len);
            if ((expected_size == 0U) || (expected_size > OTA_DOWNLOAD_SIZE))
            {
                boot_uart_putc((char)YM_CAN);
                return -1;
            }

            started = 1;
            expected_seq = 1;
            total = 0;

            /* Packet 0 is Ymodem metadata (filename/size), not file data. */
            boot_uart_putc((char)YM_ACK);
            boot_uart_putc((char)YM_C);
            last_c = boot_millis();
            continue;
        }

        if (expected_seq != (uint32_t)packet_seq)
        {
            /* Duplicate packet from an ACK lost on the wire. */
            if ((uint32_t)packet_seq == (expected_seq - 1U))
            {
                boot_uart_putc((char)YM_ACK);
                continue;
            }
            boot_uart_putc((char)YM_NAK);
            continue;
        }

        {
            uint32_t remaining = expected_size - total;
            uint32_t chunk = (packet_len < remaining) ? packet_len : remaining;

            if (chunk > 0U)
            {
                if (boot_flash_write_range(OTA_DOWNLOAD_BASE, total, packet, chunk) != 0)
                {
                    boot_uart_putc((char)YM_CAN);
                    return -1;
                }
                total += chunk;
            }

            expected_seq++;
            boot_uart_putc((char)YM_ACK);

            if (total >= expected_size)
            {
                /* The sender follows with EOT; keep waiting for it. */
            }
        }
    }
}
