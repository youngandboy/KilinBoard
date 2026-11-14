#include <rtthread.h>
#include "svc_storage.h"

static int cmd_cfg(int argc, char **argv)
{
    if (argc == 1)
    {
        svc_storage_dump();
        return 0;
    }

    if (rt_strcmp(argv[1], "save") == 0)
    {
        if (svc_storage_save() != 0)
        {
            rt_kprintf("cfg save failed\n");
            return -1;
        }
        rt_kprintf("cfg saved\n");
        return 0;
    }

    if (rt_strcmp(argv[1], "reset") == 0)
    {
        if (svc_storage_reset() != 0)
        {
            rt_kprintf("cfg reset failed\n");
            return -1;
        }
        rt_kprintf("cfg reset to defaults\n");
        svc_storage_dump();
        return 0;
    }

    if (rt_strcmp(argv[1], "set") == 0)
    {
        if (argc != 4)
        {
            rt_kprintf("usage: cfg set <wifi_ssid|wifi_pwd|srv_host|srv_port|upload_period> <value>\n");
            return -1;
        }

        if (svc_storage_set(argv[2], argv[3]) != 0)
        {
            rt_kprintf("cfg set failed\n");
            return -1;
        }

        rt_kprintf("cfg set %s = %s (use 'cfg save' to persist)\n", argv[2], argv[3]);
        return 0;
    }

    rt_kprintf("usage: cfg [save|reset|set <key> <value>]\n");
    return -1;
}
MSH_CMD_EXPORT_ALIAS(cmd_cfg, cfg, show/save/reset persistent config);
