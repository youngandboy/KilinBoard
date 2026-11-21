#include <rtthread.h>
#include "app_config.h"
#include "app_types.h"
#include "app_ipc.h"
#include "board_led.h"
#include "board_key.h"
#include "svc_sensor.h"
#include "svc_storage.h"
#include "svc_desk.h"
#include "drv_adxl362.h"
#include "task_sensor.h"

static struct rt_thread sensor_thread;
static rt_uint8_t sensor_stack[APP_TASK_SENSOR_STACK] __attribute__((aligned(RT_ALIGN_SIZE)));

static void task_sensor_entry(void *parameter)
{
    rt_uint8_t prev_keys;
    rt_tick_t last_sample;
    rt_tick_t last_tx;
    int i;

    (void)parameter;

    board_key_init();
    prev_keys = board_key_scan();
    last_sample = rt_tick_get();
    last_tx = last_sample;

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

                rt_uint8_t pressed = (keys & bit) ? 1U : 0U;

                rt_kprintf("[key] %d %s\n", i + 1, pressed ? "down" : "up");
                svc_desk_on_key((rt_uint8_t)i, pressed);
            }
            prev_keys = keys;
        }

        app_event_send(APP_EVT_SENSOR_ALIVE);

        {
            rt_tick_t now = rt_tick_get();

            svc_desk_tick(now);

            if ((now - last_sample) >=
                rt_tick_from_millisecond(APP_DESK_SAMPLE_MS))
            {
                const app_cfg_t *cfg = svc_storage_get();
                rt_uint32_t period_ms = APP_SENSOR_PERIOD_MS;
                app_sensor_sample_t sample;
                int sample_ok;

                if ((cfg != RT_NULL) && (cfg->upload_period_s > 0))
                {
                    period_ms = (rt_uint32_t)cfg->upload_period_s * 1000U;
                }

                /* Desk mode needs responsive timer/environment feedback. */
                if (svc_desk_mode() != DESK_MODE_ENV)
                {
                    period_ms = APP_DESK_SAMPLE_MS;
                }

                sample_ok = svc_sensor_sample(&sample);
                svc_desk_update_sensor(&sample, now);
                svc_desk_fill_sample(&sample);

                if ((sample_ok == APP_OK) &&
                    ((now - last_tx) >= rt_tick_from_millisecond(period_ms)))
                {
                    app_msg_t msg;
                    int temp_frac = sample.temp_x10 % 10;

                    if (temp_frac < 0)
                    {
                        temp_frac = -temp_frac;
                    }

                    rt_memset(&msg, 0, sizeof(msg));
                    msg.type = APP_MSG_SENSOR;
                    msg.u.sensor.temp_x10 = sample.temp_x10;
                    msg.u.sensor.humi_x10 = sample.humi_x10;
                    msg.u.sensor.light = sample.light_mv;
                    msg.u.sensor.acc_x_mg = sample.acc_x_mg;
                    msg.u.sensor.acc_y_mg = sample.acc_y_mg;
                    msg.u.sensor.acc_z_mg = sample.acc_z_mg;
                    msg.u.sensor.pitch_x10 = sample.pitch_x10;
                    msg.u.sensor.roll_x10 = sample.roll_x10;
                    msg.u.sensor.desk_mode = sample.desk_mode;
                    msg.u.sensor.desk_phase = sample.desk_phase;
                    msg.u.sensor.desk_remain = sample.desk_remain;
                    msg.u.sensor.desk_cycles = sample.desk_cycles;
                    msg.u.sensor.desk_sit = sample.desk_sit;
                    msg.u.sensor.desk_alert = sample.desk_alert;
                    msg.u.sensor.desk_mute = sample.desk_mute;
                    msg.u.sensor.desk_comfort = sample.desk_comfort;
                    msg.u.sensor.valid = sample.valid;

                    rt_kprintf("[sensor] T=%d.%d C, RH=%u.%u %%, light=%u mV, "
                               "acc=%d/%d/%d mg P=%d.%d R=%d.%d\n",
                               sample.temp_x10 / 10, temp_frac,
                               sample.humi_x10 / 10, sample.humi_x10 % 10,
                               sample.light_mv,
                               sample.acc_x_mg, sample.acc_y_mg, sample.acc_z_mg,
                               sample.pitch_x10 / 10, sample.pitch_x10 % 10,
                               sample.roll_x10 / 10, sample.roll_x10 % 10);

                    app_mq_send(&msg);
                    last_tx = now;
                }

                last_sample = now;
            }
        }

        /* Wake on ADXL362 INT1 data-ready instead of a fixed poll delay. */
        if (drv_adxl362_wait_data(APP_KEY_SCAN_MS) != APP_OK)
        {
            rt_thread_mdelay(APP_KEY_SCAN_MS);
        }
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
