#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <easyflash.h>
#include <fal.h>
#include "app_config.h"
#include "app_types.h"
#include "app_ipc.h"
#include "svc_storage.h"

static app_cfg_t g_cfg;
static struct rt_mutex cfg_lock;

static void cfg_set_defaults(app_cfg_t *cfg)
{
    rt_memset(cfg, 0, sizeof(*cfg));
    rt_strncpy(cfg->wifi_ssid, APP_CFG_DEFAULT_SSID, APP_CFG_SSID_MAX);
    rt_strncpy(cfg->wifi_pwd, APP_CFG_DEFAULT_PWD, APP_CFG_PWD_MAX);
    rt_strncpy(cfg->srv_host, APP_CFG_DEFAULT_HOST, APP_CFG_HOST_MAX);
    cfg->srv_port = APP_CFG_DEFAULT_PORT;
    cfg->upload_period_s = APP_CFG_DEFAULT_PERIOD_S;
}

static void load_str(const char *key, char *dst, rt_size_t size, const char *def)
{
    const char *value = ef_get_env(key);

    if ((value != RT_NULL) && (value[0] != '\0'))
    {
        rt_strncpy(dst, value, size - 1);
        dst[size - 1] = '\0';
    }
    else
    {
        rt_strncpy(dst, def, size - 1);
        dst[size - 1] = '\0';
    }
}

static rt_uint16_t load_u16(const char *key, rt_uint16_t def)
{
    const char *value = ef_get_env(key);
    long parsed;

    if ((value == RT_NULL) || (value[0] == '\0'))
    {
        return def;
    }

    parsed = atol(value);
    if ((parsed <= 0) || (parsed > 65535))
    {
        return def;
    }

    return (rt_uint16_t)parsed;
}

static void cfg_load(app_cfg_t *cfg)
{
    load_str("wifi_ssid", cfg->wifi_ssid, sizeof(cfg->wifi_ssid), APP_CFG_DEFAULT_SSID);
    load_str("wifi_pwd", cfg->wifi_pwd, sizeof(cfg->wifi_pwd), APP_CFG_DEFAULT_PWD);
    load_str("srv_host", cfg->srv_host, sizeof(cfg->srv_host), APP_CFG_DEFAULT_HOST);
    cfg->srv_port = load_u16("srv_port", APP_CFG_DEFAULT_PORT);
    cfg->upload_period_s = load_u16("upload_period", APP_CFG_DEFAULT_PERIOD_S);
}

int svc_storage_init(void)
{
    rt_err_t result;
    EfErrCode ef_result;

    result = rt_mutex_init(&cfg_lock, "cfg", RT_IPC_FLAG_PRIO);
    if (result != RT_EOK)
    {
        return APP_ERR;
    }

    if (fal_init() < 0)
    {
        rt_kprintf("[cfg] fal_init failed\n");
        return APP_ERR;
    }

    ef_result = easyflash_init();
    if (ef_result != EF_NO_ERR)
    {
        rt_kprintf("[cfg] easyflash_init failed: %d\n", ef_result);
        return APP_ERR;
    }

    cfg_set_defaults(&g_cfg);
    cfg_load(&g_cfg);

    return APP_OK;
}

const app_cfg_t *svc_storage_get(void)
{
    return &g_cfg;
}

static int save_key(const char *key, const char *value)
{
    EfErrCode result = ef_set_env(key, value);

    if (result != EF_NO_ERR)
    {
        rt_kprintf("[cfg] set %s failed: %d\n", key, result);
        return APP_ERR;
    }

    return APP_OK;
}

int svc_storage_set(const char *key, const char *value)
{
    int result = APP_OK;

    if ((key == RT_NULL) || (value == RT_NULL))
    {
        return APP_ERR_PARAM;
    }

    rt_mutex_take(&cfg_lock, RT_WAITING_FOREVER);

    if (rt_strcmp(key, "wifi_ssid") == 0)
    {
        rt_strncpy(g_cfg.wifi_ssid, value, APP_CFG_SSID_MAX);
        g_cfg.wifi_ssid[APP_CFG_SSID_MAX] = '\0';
        result = save_key(key, g_cfg.wifi_ssid);
    }
    else if (rt_strcmp(key, "wifi_pwd") == 0)
    {
        rt_strncpy(g_cfg.wifi_pwd, value, APP_CFG_PWD_MAX);
        g_cfg.wifi_pwd[APP_CFG_PWD_MAX] = '\0';
        result = save_key(key, g_cfg.wifi_pwd);
    }
    else if (rt_strcmp(key, "srv_host") == 0)
    {
        rt_strncpy(g_cfg.srv_host, value, APP_CFG_HOST_MAX);
        g_cfg.srv_host[APP_CFG_HOST_MAX] = '\0';
        result = save_key(key, g_cfg.srv_host);
    }
    else if (rt_strcmp(key, "srv_port") == 0)
    {
        long port = atol(value);
        if ((port <= 0) || (port > 65535))
        {
            result = APP_ERR_PARAM;
        }
        else
        {
            g_cfg.srv_port = (rt_uint16_t)port;
            result = save_key(key, value);
        }
    }
    else if (rt_strcmp(key, "upload_period") == 0)
    {
        long period = atol(value);
        if ((period <= 0) || (period > 3600))
        {
            result = APP_ERR_PARAM;
        }
        else
        {
            g_cfg.upload_period_s = (rt_uint16_t)period;
            result = save_key(key, value);
        }
    }
    else
    {
        result = APP_ERR_PARAM;
    }

    rt_mutex_release(&cfg_lock);

    if (result == APP_OK)
    {
        app_event_send(APP_EVT_CFG_CHANGED);
    }

    return result;
}

int svc_storage_save(void)
{
    EfErrCode result;

    rt_mutex_take(&cfg_lock, RT_WAITING_FOREVER);
    result = ef_save_env();
    rt_mutex_release(&cfg_lock);

    if (result != EF_NO_ERR)
    {
        rt_kprintf("[cfg] save failed: %d\n", result);
        return APP_ERR;
    }

    return APP_OK;
}

int svc_storage_reset(void)
{
    app_cfg_t defaults;
    int result = APP_OK;

    cfg_set_defaults(&defaults);

    rt_mutex_take(&cfg_lock, RT_WAITING_FOREVER);
    g_cfg = defaults;

    if (save_key("wifi_ssid", g_cfg.wifi_ssid) != APP_OK) result = APP_ERR;
    if (save_key("wifi_pwd", g_cfg.wifi_pwd) != APP_OK) result = APP_ERR;
    if (save_key("srv_host", g_cfg.srv_host) != APP_OK) result = APP_ERR;

    {
        char buf[8];
        rt_snprintf(buf, sizeof(buf), "%u", g_cfg.srv_port);
        if (save_key("srv_port", buf) != APP_OK) result = APP_ERR;
        rt_snprintf(buf, sizeof(buf), "%u", g_cfg.upload_period_s);
        if (save_key("upload_period", buf) != APP_OK) result = APP_ERR;
    }

    if (ef_save_env() != EF_NO_ERR) result = APP_ERR;
    rt_mutex_release(&cfg_lock);

    return result;
}

void svc_storage_dump(void)
{
    rt_kprintf("wifi_ssid     : %s\n", g_cfg.wifi_ssid);
    rt_kprintf("wifi_pwd      : %s\n", (g_cfg.wifi_pwd[0] != '\0') ? "******" : "");
    rt_kprintf("srv_host      : %s\n", g_cfg.srv_host);
    rt_kprintf("srv_port      : %u\n", g_cfg.srv_port);
    rt_kprintf("upload_period : %u s\n", g_cfg.upload_period_s);
}
