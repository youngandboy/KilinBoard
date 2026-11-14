#include <rtthread.h>
#include <rtdevice.h>
#include <at_device_esp8266.h>
#include <netdev.h>
#include "app_config.h"
#include "app_types.h"
#include "svc_storage.h"
#include "svc_net.h"

static struct at_device_esp8266 esp0;
static int net_registered;

void svc_net_reset(void)
{
    rt_pin_mode(APP_ESP8266_RESET_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(APP_ESP8266_RESET_PIN, PIN_LOW);
    rt_thread_mdelay(100);
    rt_pin_write(APP_ESP8266_RESET_PIN, PIN_HIGH);
    rt_thread_mdelay(1500);
}

int svc_net_init(void)
{
    const app_cfg_t *cfg = svc_storage_get();

    if (cfg->wifi_ssid[0] == '\0')
    {
        rt_kprintf("[net] no wifi config, skip at_device init\n");
        return APP_OK;
    }

    svc_net_reset();

    esp0.device_name   = "esp0";
    esp0.client_name   = APP_ESP8266_UART;
    esp0.wifi_ssid     = (char *)cfg->wifi_ssid;
    esp0.wifi_password = (char *)cfg->wifi_pwd;
    esp0.recv_line_num = 512;
    esp0.user_data     = RT_NULL;

    if (at_device_register(&esp0.device,
                           "esp0",
                           APP_ESP8266_UART,
                           AT_DEVICE_CLASS_ESP8266,
                           &esp0) != RT_EOK)
    {
        rt_kprintf("[net] at_device_register failed\n");
        return APP_ERR;
    }

    net_registered = 1;
    rt_kprintf("[net] esp0 registered, ssid=%s\n", cfg->wifi_ssid);

    return APP_OK;
}

int svc_net_is_ready(void)
{
    struct netdev *dev;

    if (!net_registered)
    {
        return 0;
    }

    dev = netdev_get_by_name("esp0");
    if (dev == RT_NULL)
    {
        return 0;
    }

    return netdev_is_link_up(dev);
}
