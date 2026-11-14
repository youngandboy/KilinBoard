#include <rtthread.h>
#include <rtdevice.h>
#include "app_config.h"
#include "app_types.h"
#include "drv_light.h"

static rt_adc_device_t adc_dev;

int drv_light_init(void)
{
    adc_dev = (rt_adc_device_t)rt_device_find(APP_LIGHT_ADC_DEV);
    if (adc_dev == RT_NULL)
    {
        rt_kprintf("[light] adc %s not found\n", APP_LIGHT_ADC_DEV);
        return APP_ERR;
    }

    if (rt_adc_enable(adc_dev, APP_LIGHT_ADC_CHANNEL) != RT_EOK)
    {
        rt_kprintf("[light] adc enable failed\n");
        return APP_ERR;
    }

    return APP_OK;
}

int drv_light_read(rt_uint16_t *millivolt)
{
    rt_uint32_t raw;

    if (millivolt == RT_NULL)
    {
        return APP_ERR_PARAM;
    }
    if (adc_dev == RT_NULL)
    {
        return APP_ERR;
    }

    raw = rt_adc_read(adc_dev, APP_LIGHT_ADC_CHANNEL);
    *millivolt = (rt_uint16_t)((raw * 3300U) / 4095U);

    return APP_OK;
}
