#include <rtthread.h>
#include "app_config.h"
#include "app_types.h"
#include "app_ipc.h"
#include "board_led.h"
#include "board_buzzer.h"
#include "svc_net.h"
#include "svc_protocol.h"
#include "svc_storage.h"
#include "task_net.h"

static struct rt_thread net_thread;
static rt_uint8_t net_stack[APP_TASK_NET_STACK] __attribute__((aligned(RT_ALIGN_SIZE)));

static char rx_line[256];
static rt_size_t rx_used;
static char tx_buf[256];
static rt_uint32_t tx_seq;
static app_net_stats_t net_stats;
static rt_uint32_t reconnect_delay_ms = APP_NET_RECONNECT_MIN_MS;

static void handle_line(char *line)
{
    app_cmd_t cmd;

    if (svc_protocol_decode_cmd(line, &cmd) != APP_OK)
    {
        net_stats.last_error = APP_ERR_PARAM;
        return;
    }

    net_stats.rx_commands++;

    switch (cmd.type)
    {
    case APP_CMD_LED:
        if ((cmd.a >= 0) && (cmd.a < BOARD_LED_MAX))
        {
            board_led_write((board_led_id_t)cmd.a, cmd.b ? 1 : 0);
            rt_kprintf("[net] cmd led %d -> %d\n", cmd.a, cmd.b ? 1 : 0);
        }
        break;

    case APP_CMD_BEEP:
        if ((cmd.a > 0) && (cmd.a <= 2000))
        {
            rt_kprintf("[net] cmd beep %d ms\n", cmd.a);
            board_buzzer_beep((rt_uint32_t)cmd.a);
        }
        break;

    case APP_CMD_PERIOD:
        if ((cmd.a > 0) && (cmd.a <= 3600))
        {
            char value[8];

            rt_snprintf(value, sizeof(value), "%d", cmd.a);
            svc_storage_set("upload_period", value);
            rt_kprintf("[net] cmd period %d s\n", cmd.a);
        }
        break;

    default:
        break;
    }
}

static void feed_rx(const char *data, rt_size_t len)
{
    rt_size_t i;

    for (i = 0; i < len; i++)
    {
        char ch = data[i];

        if (ch == '\n')
        {
            rx_line[rx_used] = '\0';
            if (rx_used > 0)
            {
                handle_line(rx_line);
            }
            rx_used = 0;
        }
        else if (ch != '\r')
        {
            if (rx_used < (sizeof(rx_line) - 1))
            {
                rx_line[rx_used++] = ch;
            }
            else
            {
                rx_used = 0;
            }
        }
    }
}

static void net_drop(void)
{
    svc_net_socket_close();
    net_stats.disconnects++;
}

static void net_discard_pending(void)
{
    app_msg_t msg;

    while (app_mq_recv(&msg, 0) > 0)
    {
        if (msg.type == APP_MSG_SENSOR)
        {
            net_stats.tx_dropped++;
        }
    }
}

static void net_backoff(void)
{
    net_stats.backoff_ms = reconnect_delay_ms;
    rt_thread_mdelay((rt_int32_t)reconnect_delay_ms);

    reconnect_delay_ms *= 2U;
    if (reconnect_delay_ms > APP_NET_RECONNECT_MAX_MS)
    {
        reconnect_delay_ms = APP_NET_RECONNECT_MAX_MS;
    }
}

static void task_net_entry(void *parameter)
{
    app_msg_t msg;
    char rx_buf[128];
    const app_cfg_t *cfg;

    (void)parameter;

    while (1)
    {
        /* Keep a task heartbeat even while offline. The watchdog is gated on
         * sensor health, so a network outage does not reset the board. */
        app_event_send(APP_EVT_NET_ALIVE);

        if (!svc_net_is_ready())
        {
            if (svc_net_socket_is_connected())
            {
                net_drop();
            }
            net_discard_pending();
            rt_thread_mdelay(APP_NET_LINK_POLL_MS);
            continue;
        }

        if (!svc_net_socket_is_connected())
        {
            cfg = svc_storage_get();
            net_stats.connect_attempts++;

            if ((cfg == RT_NULL) || (cfg->srv_host[0] == '\0') || (cfg->srv_port == 0))
            {
                net_stats.last_error = APP_ERR_PARAM;
                net_discard_pending();
                net_backoff();
                continue;
            }

            if (svc_net_socket_connect(cfg->srv_host, cfg->srv_port) != APP_OK)
            {
                net_stats.last_error = APP_ERR_IO;
                net_discard_pending();
                net_backoff();
                continue;
            }

            net_stats.connect_ok++;
            net_stats.last_connect_tick = rt_tick_get();
            net_stats.last_error = 0;
            reconnect_delay_ms = APP_NET_RECONNECT_MIN_MS;
            net_stats.backoff_ms = 0;

            /* Do not replay a backlog of stale samples after reconnect. */
            net_discard_pending();
        }

        app_event_send(APP_EVT_NET_ALIVE | APP_EVT_NET_CONNECTED);

        {
            int send_failed = 0;

            while (app_mq_recv(&msg, 0) > 0)
            {
                if (msg.type == APP_MSG_SENSOR)
                {
                    app_sensor_sample_t sample;
                    int len;
                    int sent;

                    sample.temp_x10 = msg.u.sensor.temp_x10;
                    sample.humi_x10 = msg.u.sensor.humi_x10;
                    sample.light_mv = msg.u.sensor.light;
                    sample.acc_x_mg = msg.u.sensor.acc_x_mg;
                    sample.acc_y_mg = msg.u.sensor.acc_y_mg;
                    sample.acc_z_mg = msg.u.sensor.acc_z_mg;
                    sample.valid = msg.u.sensor.valid;

                    len = svc_protocol_encode_sensor(tx_buf, sizeof(tx_buf),
                                                     tx_seq++, &sample);
                    if (len <= 0)
                    {
                        net_stats.tx_errors++;
                        net_stats.last_error = APP_ERR;
                        continue;
                    }

                    sent = svc_net_socket_send(tx_buf, (rt_size_t)len);
                    if (sent <= 0)
                    {
                        net_stats.tx_errors++;
                        net_stats.last_error = APP_ERR_IO;
                        send_failed = 1;
                        break;
                    }

                    net_stats.tx_frames++;
                    net_stats.last_tx_tick = rt_tick_get();
                }
            }

            if (send_failed)
            {
                net_discard_pending();
                net_drop();
                net_backoff();
                continue;
            }
        }

        if (!svc_net_socket_is_connected())
        {
            net_drop();
            net_backoff();
            continue;
        }

        {
            int received = svc_net_socket_recv(rx_buf, sizeof(rx_buf));

            if (received > 0)
            {
                net_stats.rx_packets++;
                feed_rx(rx_buf, (rt_size_t)received);
            }
            else if (received < 0)
            {
                net_stats.rx_errors++;
                net_stats.last_error = APP_ERR_IO;
                net_discard_pending();
                net_drop();
                net_backoff();
                continue;
            }
        }

        rt_thread_mdelay(50);
    }
}

void task_net_get_stats(app_net_stats_t *stats)
{
    if (stats != RT_NULL)
    {
        *stats = net_stats;
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
