#include <rtthread.h>
#include <rtdevice.h>
#include "app_config.h"
#include "app_types.h"
#include "svc_watchdog.h"

static rt_device_t wdt_dev;

int svc_watchdog_init(void)
{
    rt_uint32_t timeout = APP_WDT_TIMEOUT_S;

    wdt_dev = rt_device_find("wdt");
    if (wdt_dev == RT_NULL)
    {
        rt_kprintf("[wdt] device not found\n");
        return APP_ERR;
    }

    if (rt_device_init(wdt_dev) != RT_EOK)
    {
        rt_kprintf("[wdt] init failed\n");
        return APP_ERR;
    }

    if (rt_device_control(wdt_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout) != RT_EOK)
    {
        rt_kprintf("[wdt] set timeout failed\n");
        return APP_ERR;
    }

    if (rt_device_control(wdt_dev, RT_DEVICE_CTRL_WDT_START, RT_NULL) != RT_EOK)
    {
        rt_kprintf("[wdt] start failed\n");
        return APP_ERR;
    }

    return APP_OK;
}

int svc_watchdog_feed(void)
{
    if (wdt_dev == RT_NULL)
    {
        return APP_ERR;
    }

    if (rt_device_control(wdt_dev, RT_DEVICE_CTRL_WDT_KEEPALIVE, RT_NULL) != RT_EOK)
    {
        return APP_ERR;
    }

    return APP_OK;
}
