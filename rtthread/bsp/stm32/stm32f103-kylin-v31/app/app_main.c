#include <rtthread.h>
#include "app_config.h"
#include "app_types.h"
#include "app_ipc.h"
#include "board_led.h"
#include "board_key.h"
#include "board_buzzer.h"
#include "svc_storage.h"
#include "svc_watchdog.h"
#include "svc_sensor.h"
#include "svc_net.h"
#include "task_sensor.h"
#include "task_supervisor.h"
#include "task_net.h"
#include "app_main.h"

int app_main(void)
{
    if (board_led_init() != APP_OK)
    {
        rt_kprintf("[app] led init failed\n");
    }

    if (board_key_init() != APP_OK)
    {
        rt_kprintf("[app] key init failed\n");
    }

    if (board_buzzer_init() != APP_OK)
    {
        rt_kprintf("[app] buzzer init failed\n");
    }

    if (app_ipc_init() != APP_OK)
    {
        rt_kprintf("[app] ipc init failed\n");
        return -1;
    }

    if (svc_storage_init() != APP_OK)
    {
        rt_kprintf("[app] storage init failed\n");
    }

    if (svc_sensor_init() != APP_OK)
    {
        rt_kprintf("[app] sensor init failed\n");
    }

    if (svc_net_init() != APP_OK)
    {
        rt_kprintf("[app] net init failed\n");
    }

    if (svc_watchdog_init() != APP_OK)
    {
        rt_kprintf("[app] watchdog init failed\n");
    }

    if (task_supervisor_start() != RT_EOK)
    {
        rt_kprintf("[app] supervisor start failed\n");
    }

    if (task_sensor_start() != RT_EOK)
    {
        rt_kprintf("[app] sensor start failed\n");
    }

    if (task_net_start() != RT_EOK)
    {
        rt_kprintf("[app] net start failed\n");
    }

    rt_kprintf("[app] started\n");
    return 0;
}
