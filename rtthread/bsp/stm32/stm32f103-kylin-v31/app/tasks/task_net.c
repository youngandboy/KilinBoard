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

static void handle_line(char *line)
{
    app_cmd_t cmd;

    if (svc_protocol_decode_cmd(line, &cmd) != APP_OK)
    {
        return;
    }

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

static void task_net_entry(void *parameter)
{
    app_msg_t msg;
    char rx_buf[128];
    const app_cfg_t *cfg;

    (void)parameter;

    while (1)
    {
        if (!svc_net_is_ready())
        {
            svc_net_socket_close();
            rt_thread_mdelay(2000);
            continue;
        }

        app_event_send(APP_EVT_NET_ALIVE);

        if (!svc_net_socket_is_connected())
        {
            cfg = svc_storage_get();
            if ((cfg->srv_host[0] == '\0') || (cfg->srv_port == 0) ||
                (svc_net_socket_connect(cfg->srv_host, cfg->srv_port) != APP_OK))
            {
                rt_thread_mdelay(3000);
                continue;
            }
        }

        app_event_send(APP_EVT_NET_ALIVE | APP_EVT_NET_CONNECTED);

        while (app_mq_recv(&msg, 0) == RT_EOK)
        {
            if (msg.type == APP_MSG_SENSOR)
            {
                app_sensor_sample_t sample;
                int len;

                sample.temp_x10 = msg.u.sensor.temp_x10;
                sample.humi_x10 = msg.u.sensor.humi_x10;
                sample.light_mv = msg.u.sensor.light;
                sample.valid = 0x03;

                len = svc_protocol_encode_sensor(tx_buf, sizeof(tx_buf),
                                                 tx_seq++, &sample);
                if (len > 0)
                {
                    if (svc_net_socket_send(tx_buf, (rt_size_t)len) < 0)
                    {
                        break;
                    }
                }
            }
        }

        if (!svc_net_socket_is_connected())
        {
            rt_thread_mdelay(500);
            continue;
        }

        {
            int received = svc_net_socket_recv(rx_buf, sizeof(rx_buf));

            if (received > 0)
            {
                feed_rx(rx_buf, (rt_size_t)received);
            }
            else if (received < 0)
            {
                svc_net_socket_close();
                rt_thread_mdelay(500);
            }
        }

        rt_thread_mdelay(50);
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
