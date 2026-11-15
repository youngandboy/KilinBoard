#include <rtthread.h>
#include "app_config.h"
#include "app_types.h"
#include "svc_ota.h"

static int cmd_ota(int argc, char **argv)
{
    ota_meta_t meta;

    if ((argc > 1) && (rt_strcmp(argv[1], "status") == 0))
    {
        if (svc_ota_status(&meta) != APP_OK)
        {
            rt_kprintf("ota: cannot read metadata\n");
            return -1;
        }

        rt_kprintf("ota: state=%u version=0x%08x image=%u crc=0x%08x\n",
                   meta.state, meta.version, meta.image_size, meta.image_crc32);
        return 0;
    }

    if ((argc > 1) && (rt_strcmp(argv[1], "start") != 0))
    {
        rt_kprintf("usage: ota [start|status]\n");
        return -1;
    }

    if (svc_ota_request() != APP_OK)
    {
        rt_kprintf("ota: cannot write request flag\n");
        return -1;
    }

    rt_kprintf("ota: rebooting to bootloader\n");
    rt_kprintf("ota: send rtthread.ota with Ymodem at 115200 8N1\n");
    rt_thread_mdelay(200);
    NVIC_SystemReset();
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_ota, ota, start or query OTA update);
