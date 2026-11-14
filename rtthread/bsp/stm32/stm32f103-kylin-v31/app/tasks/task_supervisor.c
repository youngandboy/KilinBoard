#include <rtthread.h>
#include "app_config.h"
#include "app_types.h"
#include "app_ipc.h"
#include "svc_watchdog.h"
#include "task_supervisor.h"

static struct rt_thread supervisor_thread;
static rt_uint8_t supervisor_stack[APP_TASK_SUPERVISOR_STACK] __attribute__((aligned(RT_ALIGN_SIZE)));
static volatile int feed_enabled = 1;

void task_supervisor_set_feed_enabled(int enable)
{
    feed_enabled = enable;
}

static void task_supervisor_entry(void *parameter)
{
    (void)parameter;

    while (1)
    {
        rt_uint32_t recv;

        recv = app_event_recv(APP_REQUIRED_HEALTH_MASK,
                              RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                              rt_tick_from_millisecond(APP_SUPERVISOR_PERIOD_MS));

        if ((recv & APP_REQUIRED_HEALTH_MASK) == APP_REQUIRED_HEALTH_MASK)
        {
            if (feed_enabled)
            {
                svc_watchdog_feed();
            }
        }
        else
        {
            rt_kprintf("[sup] health timeout: 0x%08x\n", recv);
        }
    }
}

int task_supervisor_start(void)
{
    rt_err_t result;

    result = rt_thread_init(&supervisor_thread,
                            "supervisor",
                            task_supervisor_entry,
                            RT_NULL,
                            supervisor_stack,
                            sizeof(supervisor_stack),
                            APP_TASK_SUPERVISOR_PRIORITY,
                            APP_TASK_SUPERVISOR_TICK);
    if (result != RT_EOK)
    {
        return APP_ERR;
    }

    return rt_thread_startup(&supervisor_thread);
}
