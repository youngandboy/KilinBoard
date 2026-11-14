#include <rtthread.h>
#include <rtdevice.h>
#include <stdlib.h>
#include "app_config.h"
#include "app_types.h"
#include "drv_at24c02.h"
#include "svc_sensor.h"

static int cmd_i2c(int argc, char **argv)
{
    struct rt_i2c_bus_device *bus;
    int addr;
    int found = 0;

    if ((argc >= 2) && (rt_strcmp(argv[1], "scan") != 0))
    {
        rt_kprintf("usage: i2c scan\n");
        return -1;
    }

    bus = (struct rt_i2c_bus_device *)rt_device_find(APP_I2C_BUS_NAME);
    if (bus == RT_NULL)
    {
        rt_kprintf("i2c: bus %s not found\n", APP_I2C_BUS_NAME);
        return -1;
    }

    rt_kprintf("i2c scan on %s:\n", APP_I2C_BUS_NAME);
    for (addr = 0x08; addr <= 0x77; addr++)
    {
        if (rt_i2c_master_send(bus, addr, 0, RT_NULL, 0) == 0)
        {
            rt_kprintf("  found 0x%02x\n", addr);
            found++;
        }
    }

    if (found == 0)
    {
        rt_kprintf("  no device\n");
    }

    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_i2c, i2c, scan I2C bus: i2c scan);

static int cmd_sensor(int argc, char **argv)
{
    app_sensor_sample_t sample;

    (void)argc;
    (void)argv;

    if (svc_sensor_sample(&sample) != APP_OK)
    {
        rt_kprintf("sensor: read failed (valid=0x%x)\n", sample.valid);
        return -1;
    }

    rt_kprintf("temp=%d.%d C, humi=%u.%u %%, light=%u mV\n",
               sample.temp_x10 / 10, abs(sample.temp_x10 % 10),
               sample.humi_x10 / 10, sample.humi_x10 % 10,
               sample.light_mv);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sensor, sensor, read SHT20 and light sensor);

static int cmd_at24(int argc, char **argv)
{
    rt_uint8_t addr;
    int i;

    if (argc < 3)
    {
        rt_kprintf("usage: at24 read <addr> <len>\n");
        rt_kprintf("       at24 write <addr> <byte0> [byte1 ... byte7]\n");
        return -1;
    }

    addr = (rt_uint8_t)strtoul(argv[2], RT_NULL, 0);

    if (rt_strcmp(argv[1], "read") == 0)
    {
        rt_uint8_t buf[32];
        int len;

        if (argc != 4)
        {
            rt_kprintf("usage: at24 read <addr> <len>\n");
            return -1;
        }

        len = atoi(argv[3]);
        if ((len <= 0) || (len > (int)sizeof(buf)))
        {
            rt_kprintf("at24 read: len must be 1..%d\n", sizeof(buf));
            return -1;
        }

        if (drv_at24c02_read(addr, buf, len) != 0)
        {
            rt_kprintf("at24 read failed\n");
            return -1;
        }

        rt_kprintf("at24[0x%02x]:", addr);
        for (i = 0; i < len; i++)
        {
            rt_kprintf(" %02x", buf[i]);
        }
        rt_kprintf("\n");
        return 0;
    }

    if (rt_strcmp(argv[1], "write") == 0)
    {
        rt_uint8_t buf[8];
        int len = argc - 3;

        if ((len <= 0) || (len > (int)sizeof(buf)))
        {
            rt_kprintf("at24 write: 1..%d bytes\n", sizeof(buf));
            return -1;
        }

        for (i = 0; i < len; i++)
        {
            buf[i] = (rt_uint8_t)strtoul(argv[3 + i], RT_NULL, 0);
        }

        if (drv_at24c02_write(addr, buf, len) != 0)
        {
            rt_kprintf("at24 write failed\n");
            return -1;
        }

        rt_kprintf("at24 write ok\n");
        return 0;
    }

    rt_kprintf("usage: at24 read|write ...\n");
    return -1;
}
MSH_CMD_EXPORT_ALIAS(cmd_at24, at24, read/write AT24C02);
