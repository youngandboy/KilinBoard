#include <rtthread.h>
#include <rtdevice.h>
#include "app_config.h"

static rt_device_t esp_uart;

static int esp_uart_open(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    if (esp_uart != RT_NULL)
    {
        return 0;
    }

    esp_uart = rt_device_find(APP_ESP8266_UART);
    if (esp_uart == RT_NULL)
    {
        rt_kprintf("esp: %s not found\n", APP_ESP8266_UART);
        return -1;
    }

    config.baud_rate = APP_ESP8266_BAUD;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
    config.parity    = PARITY_NONE;
    config.bufsz     = 256;

    if (rt_device_control(esp_uart, RT_DEVICE_CTRL_CONFIG, &config) != RT_EOK)
    {
        rt_kprintf("esp: uart config failed\n");
        return -1;
    }

    if (rt_device_open(esp_uart, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX) != RT_EOK)
    {
        rt_kprintf("esp: uart open failed\n");
        return -1;
    }

    return 0;
}

static void esp_reset(void)
{
    rt_pin_mode(APP_ESP8266_RESET_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(APP_ESP8266_RESET_PIN, PIN_LOW);
    rt_thread_mdelay(100);
    rt_pin_write(APP_ESP8266_RESET_PIN, PIN_HIGH);
    rt_thread_mdelay(1500);
}

static void esp_read_response(rt_uint32_t ms)
{
    char buf[129];
    rt_tick_t start = rt_tick_get();
    rt_bool_t got = RT_FALSE;

    while ((rt_tick_get() - start) < rt_tick_from_millisecond(ms))
    {
        rt_ssize_t len = rt_device_read(esp_uart, -1, buf, sizeof(buf) - 1);

        if (len > 0)
        {
            buf[len] = '\0';
            rt_kprintf("%s", buf);
            got = RT_TRUE;
        }
        else
        {
            rt_thread_mdelay(10);
        }
    }

    if (!got)
    {
        rt_kprintf("esp: no response\n");
    }
    else
    {
        rt_kprintf("\n");
    }
}

static int cmd_esp(int argc, char **argv)
{
    char cmd[128];
    int pos = 0;
    int i;

    if (esp_uart_open() != 0)
    {
        return -1;
    }

    if (argc == 2 && rt_strcmp(argv[1], "reset") == 0)
    {
        rt_kprintf("esp: reset on PA0\n");
        esp_reset();
        rt_kprintf("esp: ready\n");
        return 0;
    }

    if (argc < 2)
    {
        rt_kprintf("usage: esp reset\n");
        rt_kprintf("       esp AT+GMR\n");
        rt_kprintf("       esp AT+CWMODE=1\n");
        return -1;
    }

    for (i = 1; i < argc; i++)
    {
        int n = rt_strlen(argv[i]);
        if ((pos + n + 2) >= (int)sizeof(cmd))
        {
            rt_kprintf("esp: command too long\n");
            return -1;
        }
        if (i > 1)
        {
            cmd[pos++] = ' ';
        }
        rt_memcpy(&cmd[pos], argv[i], n);
        pos += n;
    }
    cmd[pos++] = '\r';
    cmd[pos++] = '\n';

    rt_kprintf("esp> %.*s\n", pos - 2, cmd);
    rt_device_write(esp_uart, 0, cmd, pos);
    esp_read_response(2000);

    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_esp, esp, ESP8266 AT probe: esp reset / esp AT+GMR);
