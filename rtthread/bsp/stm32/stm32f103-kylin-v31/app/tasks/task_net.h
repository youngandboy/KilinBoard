#ifndef TASK_NET_H__
#define TASK_NET_H__

#include <rtthread.h>

typedef struct
{
    rt_uint32_t connect_attempts;
    rt_uint32_t connect_ok;
    rt_uint32_t disconnects;
    rt_uint32_t tx_frames;
    rt_uint32_t tx_errors;
    rt_uint32_t tx_dropped;
    rt_uint32_t rx_packets;
    rt_uint32_t rx_commands;
    rt_uint32_t rx_errors;
    rt_uint32_t backoff_ms;
    rt_int32_t last_error;
    rt_tick_t last_connect_tick;
    rt_tick_t last_tx_tick;
} app_net_stats_t;

int task_net_start(void);
void task_net_get_stats(app_net_stats_t *stats);

#endif /* TASK_NET_H__ */
