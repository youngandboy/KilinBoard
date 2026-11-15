#include <rtthread.h>
#include "app_types.h"
#include "drv_adxl362.h"

static int cmd_adxl(int argc, char **argv)
{
    if ((argc > 1) && (rt_strcmp(argv[1], "id") == 0))
    {
        rt_uint8_t devid = 0;
        rt_uint8_t partid = 0;

        if (drv_adxl362_read_id(&devid, &partid) != APP_OK)
        {
            rt_kprintf("adxl: id read failed\n");
            return -1;
        }
        rt_kprintf("adxl: devid=0x%02x partid=0x%02x\n", devid, partid);
        return 0;
    }

    if ((argc > 1) && (rt_strcmp(argv[1], "cal") == 0))
    {
        rt_int16_t ox = 0;
        rt_int16_t oy = 0;
        rt_int16_t oz = 0;

        if (drv_adxl362_calibrate(32) != APP_OK)
        {
            rt_kprintf("adxl: calibration failed\n");
            return -1;
        }
        drv_adxl362_get_offsets(&ox, &oy, &oz);
        rt_kprintf("adxl: calibrated offsets X=%d Y=%d Z=%d mg\n", ox, oy, oz);
        return 0;
    }

    if ((argc > 1) && (rt_strcmp(argv[1], "reset") == 0))
    {
        drv_adxl362_clear_calibration();
        rt_kprintf("adxl: calibration cleared\n");
        return 0;
    }

    if ((argc > 1) && (rt_strcmp(argv[1], "irq") == 0))
    {
        rt_kprintf("adxl: INT1 data-ready count=%u, INT2 data-ready count=%u\n",
                   (unsigned int)drv_adxl362_irq_count(),
                   (unsigned int)drv_adxl362_int2_irq_count());
        return 0;
    }

    if ((argc > 1) && (rt_strcmp(argv[1], "fifo") == 0))
    {
        adxl362_xyz_t samples[16];
        rt_uint32_t count = 0;

        if (drv_adxl362_read_fifo(samples, 16, &count) != APP_OK)
        {
            rt_kprintf("adxl: fifo read failed\n");
            return -1;
        }
        rt_kprintf("adxl: fifo samples=%u", (unsigned int)count);
        if (count > 0)
        {
            rt_kprintf(" first=%d/%d/%d last=%d/%d/%d mg",
                       samples[0].x_mg, samples[0].y_mg, samples[0].z_mg,
                       samples[count - 1].x_mg,
                       samples[count - 1].y_mg,
                       samples[count - 1].z_mg);
        }
        rt_kprintf("\n");
        return 0;
    }

    if ((argc > 1) && (rt_strcmp(argv[1], "read") != 0))
    {
        rt_kprintf("usage: adxl [read|id|cal|reset|irq|fifo]\n");
        return -1;
    }

    {
        adxl362_xyz_t xyz;
        int ret = drv_adxl362_read(&xyz);

        if (ret != APP_OK)
        {
            rt_kprintf("adxl: read failed: %d\n", ret);
            return -1;
        }

        rt_kprintf("adxl: X=%d mg Y=%d mg Z=%d mg\n",
                   xyz.x_mg, xyz.y_mg, xyz.z_mg);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_adxl, adxl, ADXL362 id/read/cal/irq/fifo);
