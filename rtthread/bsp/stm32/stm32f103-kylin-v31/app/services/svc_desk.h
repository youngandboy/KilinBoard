#ifndef SVC_DESK_H__
#define SVC_DESK_H__

#include "app_types.h"

typedef enum
{
    DESK_MODE_ENV = 0,
    DESK_MODE_POMO,
    DESK_MODE_SIT
} desk_mode_t;

typedef enum
{
    DESK_PHASE_IDLE = 0,
    DESK_PHASE_FOCUS,
    DESK_PHASE_PAUSED,
    DESK_PHASE_BREAK,
    DESK_PHASE_LONG_BREAK
} desk_phase_t;

typedef enum
{
    DESK_COMFORT_UNKNOWN = 0,
    DESK_COMFORT_OK,
    DESK_COMFORT_HOT,
    DESK_COMFORT_COLD,
    DESK_COMFORT_DRY,
    DESK_COMFORT_HUMID
} desk_comfort_t;

typedef struct
{
    rt_uint8_t mode;
    rt_uint8_t phase;
    rt_uint8_t running;
    rt_uint8_t mute;
    rt_uint8_t focus_done;
    rt_uint8_t sit_alert;
    rt_uint16_t remain_s;
    rt_uint16_t sit_s;
    rt_uint8_t comfort;
} desk_status_t;

int svc_desk_init(void);
void svc_desk_tick(rt_tick_t now);
void svc_desk_update_sensor(const app_sensor_sample_t *sample, rt_tick_t now);
void svc_desk_on_key(rt_uint8_t id, rt_uint8_t pressed);
void svc_desk_fill_sample(app_sensor_sample_t *sample);
void svc_desk_get_status(desk_status_t *status);
desk_mode_t svc_desk_mode(void);
int svc_desk_command(const char *action, int value);
void svc_desk_beep(void);
void svc_desk_set_test(rt_uint16_t focus_s, rt_uint16_t break_s, rt_uint16_t sit_s);

#endif /* SVC_DESK_H__ */
