#include <rtthread.h>
#include <stdlib.h>
#include "app_types.h"
#include "svc_desk.h"

static const char *desk_mode_name(rt_uint8_t mode)
{
    switch (mode)
    {
    case DESK_MODE_POMO: return "pomodoro";
    case DESK_MODE_SIT:  return "sedentary";
    default:             return "environment";
    }
}

static const char *desk_phase_name(rt_uint8_t phase)
{
    switch (phase)
    {
    case DESK_PHASE_FOCUS:      return "focus";
    case DESK_PHASE_PAUSED:     return "paused";
    case DESK_PHASE_BREAK:      return "break";
    case DESK_PHASE_LONG_BREAK: return "long-break";
    default:                    return "idle";
    }
}

static const char *desk_comfort_name(rt_uint8_t comfort)
{
    switch (comfort)
    {
    case DESK_COMFORT_OK:   return "comfortable";
    case DESK_COMFORT_HOT:  return "hot";
    case DESK_COMFORT_COLD: return "cold";
    case DESK_COMFORT_DRY:  return "dry";
    case DESK_COMFORT_HUMID:return "humid";
    default:                return "unknown";
    }
}

static int parse_mode(const char *name)
{
    if (rt_strcmp(name, "env") == 0) { return DESK_MODE_ENV; }
    if (rt_strcmp(name, "pomo") == 0) { return DESK_MODE_POMO; }
    if (rt_strcmp(name, "sit") == 0) { return DESK_MODE_SIT; }
    return -1;
}

static int cmd_desk(int argc, char **argv)
{
    desk_status_t status;

    if ((argc == 1) || (rt_strcmp(argv[1], "status") == 0))
    {
        svc_desk_get_status(&status);
        rt_kprintf("desk: mode=%s phase=%s remain=%us cycles=%u mute=%u\n",
                   desk_mode_name(status.mode), desk_phase_name(status.phase),
                   status.remain_s, status.focus_done, status.mute);
        rt_kprintf("      sit=%us alert=%u comfort=%s\n",
                   status.sit_s, status.sit_alert,
                   desk_comfort_name(status.comfort));
        return 0;
    }

    if (rt_strcmp(argv[1], "mode") == 0)
    {
        int mode;

        if (argc != 3)
        {
            rt_kprintf("usage: desk mode env|pomo|sit\n");
            return -1;
        }

        mode = parse_mode(argv[2]);
        if (mode < 0)
        {
            rt_kprintf("desk: unknown mode\n");
            return -1;
        }
        return (svc_desk_command("mode", mode) == APP_OK) ? 0 : -1;
    }

    if ((rt_strcmp(argv[1], "start") == 0) ||
        (rt_strcmp(argv[1], "pause") == 0) ||
        (rt_strcmp(argv[1], "reset") == 0) ||
        (rt_strcmp(argv[1], "ack") == 0) ||
        (rt_strcmp(argv[1], "beep") == 0))
    {
        return (svc_desk_command(argv[1], 0) == APP_OK) ? 0 : -1;
    }

    if (rt_strcmp(argv[1], "mute") == 0)
    {
        int value;

        if (argc != 3)
        {
            rt_kprintf("usage: desk mute 0|1\n");
            return -1;
        }
        value = atoi(argv[2]) ? 1 : 0;
        return (svc_desk_command("mute", value) == APP_OK) ? 0 : -1;
    }

    if (rt_strcmp(argv[1], "test") == 0)
    {
        if (argc != 5)
        {
            rt_kprintf("usage: desk test <focus_s> <break_s> <sit_s>\n");
            return -1;
        }

        svc_desk_set_test((rt_uint16_t)atoi(argv[2]),
                          (rt_uint16_t)atoi(argv[3]),
                          (rt_uint16_t)atoi(argv[4]));
        rt_kprintf("desk: test timings set; use 'desk start'\n");
        return 0;
    }

    rt_kprintf("usage: desk [status|mode|start|pause|reset|ack|mute|beep|test]\n");
    return -1;
}
MSH_CMD_EXPORT_ALIAS(cmd_desk, desk, desk companion status and control);
