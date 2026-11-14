#include <rtthread.h>
#include <rtdevice.h>
#include "app_config.h"
#include "app_types.h"
#include "drv_sht20.h"

#define SHT20_CMD_TEMP_NOHOLD   0xF3
#define SHT20_CMD_HUMI_NOHOLD   0xF5

static struct rt_i2c_bus_device *i2c_bus;

int drv_sht20_init(void)
{
    i2c_bus = (struct rt_i2c_bus_device *)rt_device_find(APP_I2C_BUS_NAME);
    if (i2c_bus == RT_NULL)
    {
        rt_kprintf("[sht20] i2c bus %s not found\n", APP_I2C_BUS_NAME);
        return APP_ERR;
    }

    {
        rt_uint8_t reset = 0xFE;
        rt_i2c_master_send(i2c_bus, APP_SHT20_ADDR, 0, &reset, 1);
        rt_thread_mdelay(20);
    }

    return APP_OK;
}

static int sht20_read_raw(rt_uint8_t cmd, rt_uint8_t raw[3])
{
    rt_ssize_t send_ret;
    rt_ssize_t recv_ret;

    if (i2c_bus == RT_NULL)
    {
        return APP_ERR;
    }

    send_ret = rt_i2c_master_send(i2c_bus, APP_SHT20_ADDR, 0, &cmd, 1);
    if (send_ret != 1)
    {
        rt_kprintf("[sht20] cmd %02x send ret=%d\n", cmd, (int)send_ret);
        return APP_ERR_IO;
    }

    /* SHT20 no-hold measurement time (max): 85 ms (T) / 29 ms (RH). */
    rt_thread_mdelay((cmd == SHT20_CMD_TEMP_NOHOLD) ? 100 : 50);

    recv_ret = rt_i2c_master_recv(i2c_bus, APP_SHT20_ADDR, 0, raw, 3);
    if (recv_ret != 3)
    {
        rt_kprintf("[sht20] cmd %02x recv ret=%d\n", cmd, (int)recv_ret);
        return APP_ERR_IO;
    }

    return APP_OK;
}

int drv_sht20_read(rt_int16_t *temp_x10, rt_uint16_t *humi_x10)
{
    rt_uint8_t raw[3];
    rt_uint16_t raw_temp;
    rt_uint16_t raw_humi;
    rt_int32_t temp_x100;
    rt_int32_t humi_x100;

    if ((temp_x10 == RT_NULL) || (humi_x10 == RT_NULL))
    {
        return APP_ERR_PARAM;
    }

    if (sht20_read_raw(SHT20_CMD_TEMP_NOHOLD, raw) != APP_OK)
    {
        return APP_ERR_IO;
    }
    raw_temp = ((rt_uint16_t)raw[0] << 8) | raw[1];

    if (sht20_read_raw(SHT20_CMD_HUMI_NOHOLD, raw) != APP_OK)
    {
        return APP_ERR_IO;
    }
    raw_humi = ((rt_uint16_t)raw[0] << 8) | raw[1];

    temp_x100 = -4685 + (rt_int32_t)((17572 * (rt_int32_t)raw_temp) / 65536);
    humi_x100 = -600 + (rt_int32_t)((12500 * (rt_int32_t)raw_humi) / 65536);

    if ((temp_x100 < -4000) || (temp_x100 > 12500))
    {
        return APP_ERR_IO;
    }
    if ((humi_x100 < 0) || (humi_x100 > 10000))
    {
        return APP_ERR_IO;
    }

    *temp_x10 = (rt_int16_t)(temp_x100 / 10);
    *humi_x10 = (rt_uint16_t)(humi_x100 / 10);

    return APP_OK;
}
