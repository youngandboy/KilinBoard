#include <rtthread.h>
#include "app_config.h"
#include "app_types.h"
#include "app_ipc.h"
#include "svc_net.h"
#include "task_net.h"

static struct rt_thread net_thread;
static rt_uint8_t net_stack[APP_TASK_NET_STACK] __attribute__((aligned(RT_ALIGN_SIZE)));

static void task_net_entry(void *parameter)
{
    (void)parameter;

    while (1)
    {
        if (svc_net_is_ready())
        {
            app_event_send(APP_EVT_NET_ALIVE | APP_EVT_NET_CONNECTED);
        }
        else
        {
            rt_kprintf("[net] link down\n");
        }

        rt_thread_mdelay(5000);
    }
}

int task_net_start(void)
{
    rt_err_t result;

    result = rt_thread_init(&net_thread,
                            "net",
                            task_net_entry,
                            RT_NULL,
                            net_stack,
                            sizeof(net_stack),
                            APP_TASK_NET_PRIORITY,
                            APP_TASK_NET_TICK);
    if (result != RT_EOK)
    {
        return APP_ERR;
    }

    return rt_thread_startup(&net_thread);
}
