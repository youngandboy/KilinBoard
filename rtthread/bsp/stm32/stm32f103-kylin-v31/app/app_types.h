#ifndef APP_TYPES_H__
#define APP_TYPES_H__

#include <rtthread.h>

typedef enum
{
    APP_OK = 0,
    APP_ERR = -1,
    APP_ERR_PARAM = -2,
    APP_ERR_TIMEOUT = -3,
    APP_ERR_IO = -4,
    APP_ERR_STATE = -5,
    APP_ERR_NOMEM = -6,
} app_err_t;

typedef enum
{
    APP_MSG_NONE = 0,
    APP_MSG_KEY,
    APP_MSG_SENSOR,
    APP_MSG_NET_STATUS,
    APP_MSG_CMD,
} app_msg_type_t;

typedef struct
{
    app_msg_type_t type;
    union
    {
        struct
        {
            rt_uint8_t id;
            rt_uint8_t pressed;
        } key;

        struct
        {
            rt_int16_t temp_x10;
            rt_uint16_t humi_x10;
            rt_uint16_t light;
        } sensor;

        struct
        {
            rt_uint8_t connected;
        } net;

        struct
        {
            rt_uint8_t cmd;
            rt_uint8_t arg;
        } cmd;
    } u;
} app_msg_t;

#define APP_EVT_SENSOR_ALIVE    (1UL << 0)
#define APP_EVT_NET_ALIVE       (1UL << 1)
#define APP_EVT_NET_CONNECTED   (1UL << 2)
#define APP_EVT_CFG_CHANGED     (1UL << 3)

/* Step 4: add APP_EVT_NET_ALIVE to this mask. */
#define APP_REQUIRED_HEALTH_MASK (APP_EVT_SENSOR_ALIVE)

#endif /* APP_TYPES_H__ */
