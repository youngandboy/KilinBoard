#include <rtthread.h>
#include <stdlib.h>
#include "app_types.h"
#include "svc_protocol.h"

static int json_get_str(const char *line, const char *key,
                        char *value, rt_size_t size)
{
    const char *p = rt_strstr(line, key);
    rt_size_t len = 0;

    if ((p == RT_NULL) || (value == RT_NULL) || (size == 0U))
    {
        return -1;
    }

    p += rt_strlen(key);
    while ((*p == ' ') || (*p == ':'))
    {
        p++;
    }
    if (*p == '"')
    {
        p++;
    }

    while ((*p != '\0') && (*p != '"') && (*p != ',') && (*p != '}') &&
           (len < (size - 1U)))
    {
        value[len++] = *p++;
    }
    value[len] = '\0';
    return (len > 0U) ? 0 : -1;
}

static int json_get_int(const char *line, const char *key, int *value)
{
    const char *p = rt_strstr(line, key);

    if (p == RT_NULL)
    {
        return -1;
    }

    p += rt_strlen(key);
    while ((*p == ' ') || (*p == ':'))
    {
        p++;
    }
    if (*p == '"')
    {
        p++;
    }

    *value = atoi(p);
    return 0;
}

int svc_protocol_encode_sensor(char *buf, rt_size_t size, rt_uint32_t seq,
                               const app_sensor_sample_t *sample)
{
    int temp_frac;
    int len;
    const char *pitch_sign;
    const char *roll_sign;
    rt_uint16_t pitch_abs;
    rt_uint16_t roll_abs;

    if ((buf == RT_NULL) || (sample == RT_NULL) || (size == 0))
    {
        return APP_ERR_PARAM;
    }

    temp_frac = sample->temp_x10 % 10;
    if (temp_frac < 0)
    {
        temp_frac = -temp_frac;
    }

    pitch_sign = (sample->pitch_x10 < 0) ? "-" : "";
    pitch_abs = (sample->pitch_x10 < 0)
              ? (rt_uint16_t)(-sample->pitch_x10)
              : (rt_uint16_t)sample->pitch_x10;
    roll_sign = (sample->roll_x10 < 0) ? "-" : "";
    roll_abs = (sample->roll_x10 < 0)
             ? (rt_uint16_t)(-sample->roll_x10)
             : (rt_uint16_t)sample->roll_x10;

    len = rt_snprintf(buf, size,
                      "{\"v\":1,\"t\":\"d\",\"seq\":%u,\"up\":%u,"
                      "\"temp\":%d.%d,\"humi\":%u.%u,\"light\":%u,"
                      "\"ax\":%d,\"ay\":%d,\"az\":%d,"
                      "\"pitch\":%s%u.%u,\"roll\":%s%u.%u,"
                      "\"mode\":%u,\"phase\":%u,\"remain\":%u,"
                      "\"cycles\":%u,\"sit\":%u,\"alert\":%u,\"mute\":%u,"
                      "\"comfort\":%u,\"valid\":%u}\n",
                      (unsigned int)seq,
                      (unsigned int)(rt_tick_get() / RT_TICK_PER_SECOND),
                      sample->temp_x10 / 10, temp_frac,
                      sample->humi_x10 / 10, sample->humi_x10 % 10,
                      sample->light_mv,
                      sample->acc_x_mg, sample->acc_y_mg, sample->acc_z_mg,
                      pitch_sign, pitch_abs / 10U, pitch_abs % 10U,
                      roll_sign, roll_abs / 10U, roll_abs % 10U,
                      (unsigned int)sample->desk_mode,
                      (unsigned int)sample->desk_phase,
                      (unsigned int)sample->desk_remain,
                      (unsigned int)sample->desk_cycles,
                      (unsigned int)sample->desk_sit,
                      (unsigned int)sample->desk_alert,
                      (unsigned int)sample->desk_mute,
                      (unsigned int)sample->desk_comfort,
                      (unsigned int)sample->valid);

    if ((len < 0) || ((rt_size_t)len >= size))
    {
        return APP_ERR_NOMEM;
    }

    return len;
}

int svc_protocol_decode_cmd(const char *line, app_cmd_t *cmd)
{
    int value;

    if ((line == RT_NULL) || (cmd == RT_NULL))
    {
        return APP_ERR_PARAM;
    }

    cmd->type = APP_CMD_NONE;
    cmd->a = 0;
    cmd->b = 0;
    cmd->action[0] = '\0';

    if (rt_strstr(line, "\"desk\":") != RT_NULL)
    {
        if (json_get_str(line, "\"desk\":", cmd->action,
                         sizeof(cmd->action)) == 0)
        {
            (void)json_get_int(line, "\"v\":", &cmd->b);
            cmd->type = APP_CMD_DESK;
            return APP_OK;
        }
    }
    else if (rt_strstr(line, "\"led\":") != RT_NULL)
    {
        if ((json_get_int(line, "\"led\":", &cmd->a) == 0) &&
            (json_get_int(line, "\"on\":", &cmd->b) == 0))
        {
            cmd->type = APP_CMD_LED;
            return APP_OK;
        }
    }
    else if (rt_strstr(line, "\"beep\":") != RT_NULL)
    {
        if (json_get_int(line, "\"beep\":", &cmd->a) == 0)
        {
            cmd->type = APP_CMD_BEEP;
            return APP_OK;
        }
    }
    else if (rt_strstr(line, "\"period\":") != RT_NULL)
    {
        if (json_get_int(line, "\"period\":", &cmd->a) == 0)
        {
            cmd->type = APP_CMD_PERIOD;
            return APP_OK;
        }
    }

    return APP_ERR_PARAM;
}
