#include <rtthread.h>
#include "app_config.h"
#include "app_types.h"
#include "drv_at24c02.h"
#include "drv_sht20.h"
#include "drv_light.h"
#include "drv_adxl362.h"
#include "svc_sensor.h"

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
            sample->acc_x_mg = xyz.x_mg;
            sample->acc_y_mg = xyz.y_mg;
            sample->acc_z_mg = xyz.z_mg;
            sample->valid |= 0x04;
        }
        else
        {
            rt_kprintf("[sensor] adxl362 read failed\n");
        }
    }

    return ((sample->valid & 0x03) == 0x03) ? APP_OK : APP_ERR;
}
