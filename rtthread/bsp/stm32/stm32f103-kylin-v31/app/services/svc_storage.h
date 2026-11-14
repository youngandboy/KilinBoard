#ifndef SVC_STORAGE_H__
#define SVC_STORAGE_H__

#include <rtthread.h>
#include "app_config.h"

typedef struct
{
    char wifi_ssid[APP_CFG_SSID_MAX + 1];
    char wifi_pwd[APP_CFG_PWD_MAX + 1];
    char srv_host[APP_CFG_HOST_MAX + 1];
    rt_uint16_t srv_port;
    rt_uint16_t upload_period_s;
} app_cfg_t;

int svc_storage_init(void);
const app_cfg_t *svc_storage_get(void);

int svc_storage_set(const char *key, const char *value);
int svc_storage_save(void);
int svc_storage_reset(void);

void svc_storage_dump(void);

#endif /* SVC_STORAGE_H__ */
