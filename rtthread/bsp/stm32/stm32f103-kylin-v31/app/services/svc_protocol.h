#ifndef SVC_PROTOCOL_H__
#define SVC_PROTOCOL_H__

#include <rtthread.h>
#include "app_types.h"

typedef enum
{
    APP_CMD_NONE = 0,
    APP_CMD_LED,
    APP_CMD_BEEP,
    APP_CMD_PERIOD,
} app_cmd_type_t;

typedef struct
{
    app_cmd_type_t type;
    int a;
    int b;
} app_cmd_t;

int svc_protocol_encode_sensor(char *buf, rt_size_t size, rt_uint32_t seq,
                               const app_sensor_sample_t *sample);
int svc_protocol_decode_cmd(const char *line, app_cmd_t *cmd);

#endif /* SVC_PROTOCOL_H__ */
