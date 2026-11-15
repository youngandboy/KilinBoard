#include <rtthread.h>
#include "app_config.h"
#include "app_types.h"
#include "drv_at24c02.h"
#include "drv_sht20.h"
#include "drv_light.h"
#include "drv_adxl362.h"
#include "svc_sensor.h"

/* atan(i / 64) in tenths of a degree, i = 0..64 (0..45 degrees). */
static const rt_uint16_t adxl_atan_tab[65] =
{
    0, 9, 18, 27, 36, 45, 54, 62, 71, 80, 89, 98, 106, 115, 123, 132,
    140, 149, 157, 165, 174, 182, 190, 198, 206, 213, 221, 229, 236, 244,
    251, 258, 266, 273, 280, 287, 294, 300, 307, 314, 320, 326, 333, 339,
    345, 351, 357, 363, 369, 374, 380, 386, 391, 396, 402, 407, 412, 417,
    422, 427, 432, 436, 441, 445, 450
};

static rt_uint32_t adxl_isqrt(rt_uint32_t value)
{
    rt_uint32_t result = 0;
    rt_uint32_t bit = 1UL << 30;

    while (bit > value)
    {
        bit >>= 2;
    }
    while (bit != 0U)
    {
        if (value >= (result + bit))
        {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

/* Integer atan2, result in tenths of a degree. */
static rt_int16_t adxl_atan2_x10(rt_int32_t y, rt_int32_t x)
{
    rt_uint32_t ax = (x < 0) ? (rt_uint32_t)(-x) : (rt_uint32_t)x;
    rt_uint32_t ay = (y < 0) ? (rt_uint32_t)(-y) : (rt_uint32_t)y;
    rt_uint32_t index;
    rt_int32_t angle;

    if ((ax == 0U) && (ay == 0U))
    {
        return 0;
    }

    if (ax >= ay)
    {
        index = (ay * 64U) / ax;
        angle = adxl_atan_tab[index];
    }
    else
    {
        index = (ax * 64U) / ay;
        angle = 900 - adxl_atan_tab[index];
    }

    if (x < 0)
    {
        angle = 1800 - angle;
    }
    if (y < 0)
    {
        angle = -angle;
    }
    return (rt_int16_t)angle;
}

int svc_sensor_init(void)
{
    int result = APP_OK;

    if (drv_at24c02_init() != APP_OK)
    {
        rt_kprintf("[sensor] at24c02 init failed\n");
        result = APP_ERR;
    }

    if (drv_sht20_init() != APP_OK)
    {
        rt_kprintf("[sensor] sht20 init failed\n");
        result = APP_ERR;
    }

    if (drv_light_init() != APP_OK)
    {
        rt_kprintf("[sensor] light init failed\n");
        result = APP_ERR;
    }

    if (drv_adxl362_init() != APP_OK)
    {
        rt_kprintf("[sensor] adxl362 init failed\n");
        result = APP_ERR;
    }

    return result;
}

int svc_sensor_sample(app_sensor_sample_t *sample)
{
    if (sample == RT_NULL)
    {
        return APP_ERR_PARAM;
    }

    rt_memset(sample, 0, sizeof(*sample));

    if (drv_sht20_read(&sample->temp_x10, &sample->humi_x10) == APP_OK)
    {
        sample->valid |= 0x01;
    }
    else
    {
        rt_kprintf("[sensor] sht20 read failed\n");
    }

    if (drv_light_read(&sample->light_mv) == APP_OK)
    {
        sample->valid |= 0x02;
    }
    else
    {
        rt_kprintf("[sensor] light read failed\n");
    }

    {
        adxl362_xyz_t xyz;

        if (drv_adxl362_read(&xyz) == APP_OK)
        {
            rt_int32_t ax = xyz.x_mg;
            rt_int32_t ay = xyz.y_mg;
            rt_int32_t az = xyz.z_mg;

            sample->acc_x_mg = xyz.x_mg;
            sample->acc_y_mg = xyz.y_mg;
            sample->acc_z_mg = xyz.z_mg;
            sample->pitch_x10 = adxl_atan2_x10(
                ay, (rt_int32_t)adxl_isqrt((rt_uint32_t)(ax * ax + az * az)));
            sample->roll_x10 = adxl_atan2_x10(
                ax, (rt_int32_t)adxl_isqrt((rt_uint32_t)(ay * ay + az * az)));
            sample->valid |= 0x04;
        }
        else
        {
            rt_kprintf("[sensor] adxl362 read failed\n");
        }
    }

    return ((sample->valid & 0x03) == 0x03) ? APP_OK : APP_ERR;
}
