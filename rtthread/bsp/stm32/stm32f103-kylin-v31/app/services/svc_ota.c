#include <rtthread.h>
#include <fal.h>
#include "app_types.h"
#include "svc_ota.h"

static const struct fal_partition *ota_meta_partition(void)
{
    return fal_partition_find("meta");
}

int svc_ota_status(ota_meta_t *meta)
{
    const struct fal_partition *part;

    if (meta == RT_NULL)
    {
        return APP_ERR_PARAM;
    }

    part = ota_meta_partition();
    if (part == RT_NULL)
    {
        return APP_ERR_STATE;
    }

    if (fal_partition_read(part, 0, (rt_uint8_t *)meta, sizeof(*meta)) < 0)
    {
        return APP_ERR_IO;
    }

    if (meta->magic != OTA_META_MAGIC)
    {
        rt_memset(meta, 0, sizeof(*meta));
        meta->magic = OTA_META_MAGIC;
        meta->state = OTA_META_STATE_IDLE;
    }

    return APP_OK;
}

int svc_ota_request(void)
{
    const struct fal_partition *part;
    ota_meta_t meta;

    part = ota_meta_partition();
    if (part == RT_NULL)
    {
        return APP_ERR_STATE;
    }

    rt_memset(&meta, 0, sizeof(meta));
    meta.magic = OTA_META_MAGIC;
    meta.state = OTA_META_STATE_REQUEST;

    if (fal_partition_erase(part, 0, OTA_META_SIZE) < 0)
    {
        return APP_ERR_IO;
    }
    if (fal_partition_write(part, 0, (const rt_uint8_t *)&meta, sizeof(meta)) < 0)
    {
        return APP_ERR_IO;
    }

    return APP_OK;
}
