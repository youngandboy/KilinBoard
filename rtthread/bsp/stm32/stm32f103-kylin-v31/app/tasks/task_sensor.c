#include <rtthread.h>
#include "app_config.h"
#include "app_types.h"
#include "app_ipc.h"
#include "board_led.h"
#include "board_key.h"
#include "svc_sensor.h"
#include "svc_storage.h"
#include "task_sensor.h"

static struct rt_thread sensor_thread;
static rt_uint8_t sensor_stack[APP_TASK_SENSOR_STACK] __attribute__((aligned(RT_ALIGN_SIZE)));

static void task_sensor_entry(void *parameter)
{
    rt_uint8_t prev_keys;
    rt_tick_t last_sample;
    int i;

    (void)parameter;

    board_key_init();
    prev_keys = board_key_scan();
    last_sample = rt_tick_get();

    while (1)
    {
        rt_uint8_t keys = board_key_scan();

        if (keys != prev_keys)
        {
            for (i = 0; i < BOARD_KEY_MAX; i++)
            {
                rt_uint8_t bit = (1UL << i);

                if ((keys & bit) == (prev_keys & bit))
                {
                    continue;
                }

                app_msg_t msg;

                rt_memset(&msg, 0, sizeof(msg));
                msg.type = APP_MSG_KEY;
                msg.u.key.id = (rt_uint8_t)i;
                msg.u.key.pressed = (keys & bit) ? 1 : 0;

                rt_kprintf("[key] %d %s\n", i + 1, msg.u.key.pressed ? "down" : "up");
                board_led_toggle((board_led_id_t)i);
                app_mq_send(&msg);
            }
            prev_keys = keys;
        }

        app_event_send(APP_EVT_SENSOR_ALIVE);

        {
            const app_cfg_t *cfg = svc_storage_get();
            rt_uint32_t period_ms = APP_SENSOR_PERIOD_MS;

            if ((cfg != RT_NULL) && (cfg->upload_period_s > 0))
            {
                period_ms = (rt_uint32_t)cfg->upload_period_s * 1000U;
            }

            if ((rt_tick_get() - last_sample) >= rt_tick_from_millisecond(period_ms))
            {
                app_msg_t msg;
                app_sensor_sample_t sample;

                rt_memset(&msg, 0, sizeof(msg));
                msg.type = APP_MSG_SENSOR;

                if (svc_sensor_sample(&sample) == APP_OK)
                {
                    int temp_frac = sample.temp_x10 % 10;

                    if (temp_frac < 0)
                    {
                        temp_frac = -temp_frac;
                    }

                    msg.u.sensor.temp_x10 = sample.temp_x10;
                    msg.u.sensor.humi_x10 = sample.humi_x10;
                    msg.u.sensor.light = sample.light_mv;
                    msg.u.sensor.acc_x_mg = sample.acc_x_mg;
                    msg.u.sensor.acc_y_mg = sample.acc_y_mg;
                    msg.u.sensor.acc_z_mg = sample.acc_z_mg;
                    msg.u.sensor.valid = sample.valid;

                    rt_kprintf("[sensor] T=%d.%d C, RH=%u.%u %%, light=%u mV, "
                               "acc=%d/%d/%d mg\n",
                               sample.temp_x10 / 10, temp_frac,
                               sample.humi_x10 / 10, sample.humi_x10 % 10,
                               sample.light_mv,
                               sample.acc_x_mg, sample.acc_y_mg, sample.acc_z_mg);
                }

                app_mq_send(&msg);
                last_sample = rt_tick_get();
            }
        }

        rt_thread_mdelay(APP_KEY_SCAN_MS);
    }
}

int task_sensor_start(void)
{
    rt_err_t result;

    result = rt_thread_init(&sensor_thread,
                            "sensor",
                            task_sensor_entry,
                            RT_NULL,
                            sensor_stack,
                            sizeof(sensor_stack),
                            APP_TASK_SENSOR_PRIORITY,
                            APP_TASK_SENSOR_TICK);
    if (result != RT_EOK)
    {
        return APP_ERR;
    }

    return rt_thread_startup(&sensor_thread);
}
