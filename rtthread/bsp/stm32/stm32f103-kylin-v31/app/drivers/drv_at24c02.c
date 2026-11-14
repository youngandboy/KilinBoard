#include <rtthread.h>
#include <rtdevice.h>
#include "app_config.h"
#include "app_types.h"
#include "drv_at24c02.h"

static struct rt_i2c_bus_device *i2c_bus;

int drv_at24c02_init(void)
{
    i2c_bus = (struct rt_i2c_bus_device *)rt_device_find(APP_I2C_BUS_NAME);
    if (i2c_bus == RT_NULL)
    {
        rt_kprintf("[at24] i2c bus %s not found\n", APP_I2C_BUS_NAME);
        return APP_ERR;
    }

    return APP_OK;
}

int drv_at24c02_read(rt_uint8_t mem_addr, rt_uint8_t *buf, rt_size_t len)
{
    struct rt_i2c_msg msgs[2];

    if ((i2c_bus == RT_NULL) || (buf == RT_NULL) || (len == 0))
    {
        return APP_ERR_PARAM;
    }

    msgs[0].addr  = APP_AT24C02_ADDR;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf   = &mem_addr;
    msgs[0].len   = 1;

    msgs[1].addr  = APP_AT24C02_ADDR;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf   = buf;
    msgs[1].len   = len;

    if (rt_i2c_transfer(i2c_bus, msgs, 2) != 2)
    {
        return APP_ERR_IO;
    }

    return APP_OK;
}

int drv_at24c02_write(rt_uint8_t mem_addr, const rt_uint8_t *buf, rt_size_t len)
{
    rt_size_t offset = 0;

    if ((i2c_bus == RT_NULL) || (buf == RT_NULL) || (len == 0))
    {
        return APP_ERR_PARAM;
    }

    while (offset < len)
    {
        rt_uint8_t frame[1 + APP_AT24C02_PAGE_SIZE];
        rt_size_t page_offset = (mem_addr + offset) % APP_AT24C02_PAGE_SIZE;
        rt_size_t chunk = APP_AT24C02_PAGE_SIZE - page_offset;
        struct rt_i2c_msg msg;

        if (chunk > (len - offset))
        {
            chunk = len - offset;
        }

        frame[0] = (rt_uint8_t)(mem_addr + offset);
        rt_memcpy(&frame[1], buf + offset, chunk);

        msg.addr  = APP_AT24C02_ADDR;
        msg.flags = RT_I2C_WR;
        msg.buf   = frame;
        msg.len   = chunk + 1;

        if (rt_i2c_transfer(i2c_bus, &msg, 1) != 1)
        {
            return APP_ERR_IO;
        }

        /* EEPROM internal write cycle. */
        rt_thread_mdelay(5);
        offset += chunk;
    }

    return APP_OK;
}
