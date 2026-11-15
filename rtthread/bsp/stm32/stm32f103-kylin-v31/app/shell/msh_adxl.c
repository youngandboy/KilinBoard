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

    if ((argc > 1) && (rt_strcmp(argv[1], "read") != 0))
    {
        rt_kprintf("usage: adxl [read|id]\n");
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
MSH_CMD_EXPORT_ALIAS(cmd_adxl, adxl, read ADXL362 acceleration);
