#include <rtthread.h>
#include "app_config.h"
#include "app_ipc.h"

static app_msg_t app_mq_pool[APP_MQ_DEPTH];
static struct rt_messagequeue app_mq;
static struct rt_event app_events;

int app_ipc_init(void)
{
    rt_err_t result;

    result = rt_mq_init(&app_mq,
                        "app_mq",
                        app_mq_pool,
                        sizeof(app_msg_t),
                        sizeof(app_mq_pool),
                        RT_IPC_FLAG_PRIO);
    if (result != RT_EOK)
    {
        return APP_ERR;
    }

    result = rt_event_init(&app_events, "app_evt", RT_IPC_FLAG_PRIO);
    if (result != RT_EOK)
    {
        return APP_ERR;
    }

    return APP_OK;
}

rt_err_t app_mq_send(const app_msg_t *msg)
{
    if (msg == RT_NULL)
    {
        return -RT_EINVAL;
    }

    return rt_mq_send(&app_mq, msg, sizeof(*msg));
}

rt_ssize_t app_mq_recv(app_msg_t *msg, rt_int32_t timeout)
{
    if (msg == RT_NULL)
    {
        return -RT_EINVAL;
    }

    return rt_mq_recv(&app_mq, msg, sizeof(*msg), timeout);
}

rt_err_t app_event_send(rt_uint32_t bits)
{
    return rt_event_send(&app_events, bits);
}

rt_uint32_t app_event_recv(rt_uint32_t set, rt_uint8_t option, rt_int32_t timeout)
{
    rt_uint32_t recv = 0;
    rt_err_t result;

    result = rt_event_recv(&app_events, set, option, timeout, &recv);
    if (result != RT_EOK)
    {
        return 0;
    }

    return recv;
}
