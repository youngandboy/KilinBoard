#include <rtthread.h>
#include "app_config.h"
#include "app_types.h"
#include "board_led.h"
#include "board_buzzer.h"
#include "svc_desk.h"

#define DESK_FOCUS_S        (25U * 60U)
#define DESK_BREAK_S        (5U * 60U)
#define DESK_LONG_S         (15U * 60U)
#define DESK_SIT_WARN_S     (30U * 60U)
#define DESK_SIT_ALERT_S    (45U * 60U)
#define DESK_MOTION_MG      80

#define DESK_LED_ALL        0x0FU
#define DESK_LED_RED        (1U << 0)
#define DESK_LED_GREEN      (1U << 1)
#define DESK_LED_YELLOW     (1U << 2)
#define DESK_LED_BLUE       (1U << 3)

static const rt_uint16_t beep_short_pat[] = { 120U, 0U };
static const rt_uint16_t beep_double_pat[] = { 100U, 100U, 100U, 0U };
static const rt_uint16_t beep_triple_pat[] = { 100U, 100U, 100U, 100U, 100U, 0U };
static const rt_uint16_t beep_alert_pat[] = { 300U, 200U, 300U, 200U, 300U, 0U };

static struct rt_mutex desk_lock;
static desk_mode_t current_mode;
static desk_phase_t current_phase;
static desk_phase_t resume_phase;
static rt_uint8_t running;
static rt_uint8_t muted;
static rt_uint8_t focus_done;
static rt_uint8_t sit_alert;
static rt_uint16_t remain_s;
static rt_uint16_t sit_s;
static rt_uint8_t comfort;
static rt_uint16_t focus_s;
static rt_uint16_t break_s;
static rt_uint16_t long_s;
static rt_uint16_t sit_warn_s;
static rt_uint16_t sit_alert_s;
static rt_tick_t phase_end_tick;
static rt_tick_t last_motion_tick;
static rt_tick_t mode_flash_start;
static rt_tick_t mode_flash_until;
static rt_uint8_t last_led_mask;
static rt_int16_t last_x;
static rt_int16_t last_y;
static rt_int16_t last_z;
static rt_uint8_t have_accel;
static const rt_uint16_t *beep_pattern;
static rt_uint8_t beep_index;
static rt_uint8_t beep_active;
static rt_tick_t beep_next_tick;

static rt_tick_t desk_ticks(rt_uint32_t ms)
{
    return rt_tick_from_millisecond(ms);
}

static void beep_stop(void)
{
    board_buzzer_off();
    beep_active = 0;
    beep_pattern = RT_NULL;
    beep_index = 0;
}

static void beep_start(const rt_uint16_t *pattern)
{
    if (muted || (pattern == RT_NULL))
    {
        return;
    }

    beep_pattern = pattern;
    beep_index = 0;
    beep_active = 1;
    beep_next_tick = rt_tick_get();
}

static void beep_tick(rt_tick_t now)
{
    if (!beep_active || (beep_pattern == RT_NULL))
    {
        return;
    }

    if ((rt_int32_t)(now - beep_next_tick) < 0)
    {
        return;
    }

    if (beep_pattern[beep_index] == 0U)
    {
        beep_stop();
        return;
    }

    if ((beep_index & 1U) == 0U)
    {
        board_buzzer_on();
    }
    else
    {
        board_buzzer_off();
    }

    beep_next_tick = now + desk_ticks(beep_pattern[beep_index]);
    beep_index++;
}

static void set_mode_flash(rt_tick_t now)
{
    mode_flash_start = now;
    mode_flash_until = now + desk_ticks(600U);
}

static void reset_pomodoro(rt_tick_t now)
{
    current_phase = DESK_PHASE_IDLE;
    resume_phase = DESK_PHASE_IDLE;
    running = 0;
    remain_s = focus_s;
    phase_end_tick = now;
}

static void set_pomodoro_phase(desk_phase_t phase, rt_uint16_t seconds, rt_tick_t now)
{
    current_phase = phase;
    remain_s = seconds;
    running = 1;
    phase_end_tick = now + desk_ticks((rt_uint32_t)seconds * 1000U);
}

static void start_pomodoro(rt_tick_t now)
{
    if (current_phase == DESK_PHASE_PAUSED)
    {
        current_phase = resume_phase;
        running = 1;
        phase_end_tick = now + desk_ticks((rt_uint32_t)remain_s * 1000U);
        beep_start(beep_short_pat);
    }
    else if (current_phase == DESK_PHASE_IDLE)
    {
        set_pomodoro_phase(DESK_PHASE_FOCUS, focus_s, now);
        beep_start(beep_short_pat);
    }
}

static void pause_pomodoro(rt_tick_t now)
{
    if (!running)
    {
        return;
    }

    if ((current_phase == DESK_PHASE_FOCUS) ||
        (current_phase == DESK_PHASE_BREAK) ||
        (current_phase == DESK_PHASE_LONG_BREAK))
    {
        remain_s = (rt_uint16_t)((phase_end_tick - now +
                                  desk_ticks(1000U) - 1U) /
                                 desk_ticks(1000U));
        resume_phase = current_phase;
        current_phase = DESK_PHASE_PAUSED;
        running = 0;
        beep_start(beep_double_pat);
    }
}

static void finish_pomodoro_phase(rt_tick_t now)
{
    if (current_phase == DESK_PHASE_FOCUS)
    {
        focus_done++;
        beep_start(beep_triple_pat);

        if ((focus_done % 4U) == 0U)
        {
            set_pomodoro_phase(DESK_PHASE_LONG_BREAK, long_s, now);
        }
        else
        {
            set_pomodoro_phase(DESK_PHASE_BREAK, break_s, now);
        }
    }
    else if ((current_phase == DESK_PHASE_BREAK) ||
             (current_phase == DESK_PHASE_LONG_BREAK))
    {
        beep_start(beep_double_pat);
        reset_pomodoro(now);
    }
}

static void update_comfort(const app_sensor_sample_t *sample)
{
    if ((sample == RT_NULL) || ((sample->valid & 0x03U) != 0x03U))
    {
        comfort = DESK_COMFORT_UNKNOWN;
    }
    else if (sample->temp_x10 > 280)
    {
        comfort = DESK_COMFORT_HOT;
    }
    else if (sample->temp_x10 < 200)
    {
        comfort = DESK_COMFORT_COLD;
    }
    else if (sample->humi_x10 < 400U)
    {
        comfort = DESK_COMFORT_DRY;
    }
    else if (sample->humi_x10 > 650U)
    {
        comfort = DESK_COMFORT_HUMID;
    }
    else
    {
        comfort = DESK_COMFORT_OK;
    }
}

static void update_motion(const app_sensor_sample_t *sample, rt_tick_t now)
{
    rt_int32_t dx;
    rt_int32_t dy;
    rt_int32_t dz;

    if ((sample == RT_NULL) || ((sample->valid & 0x04U) == 0U))
    {
        have_accel = 0;
        last_motion_tick = now;
        sit_alert = 0;
        return;
    }

    if (!have_accel)
    {
        have_accel = 1;
        last_x = sample->acc_x_mg;
        last_y = sample->acc_y_mg;
        last_z = sample->acc_z_mg;
        return;
    }

    dx = (rt_int32_t)sample->acc_x_mg - last_x;
    dy = (rt_int32_t)sample->acc_y_mg - last_y;
    dz = (rt_int32_t)sample->acc_z_mg - last_z;

    if (dx < 0) { dx = -dx; }
    if (dy < 0) { dy = -dy; }
    if (dz < 0) { dz = -dz; }

    if ((dx + dy + dz) >= DESK_MOTION_MG)
    {
        if (sit_alert)
        {
            beep_stop();
        }
        last_motion_tick = now;
        sit_alert = 0;
    }

    last_x = sample->acc_x_mg;
    last_y = sample->acc_y_mg;
    last_z = sample->acc_z_mg;
}

static void render_leds(rt_tick_t now)
{
    rt_uint8_t mask = 0U;

    if ((rt_int32_t)(now - mode_flash_until) < 0)
    {
        mask = (((now - mode_flash_start) / desk_ticks(150U)) & 1U)
             ? DESK_LED_ALL : 0U;
    }
    else if (current_mode == DESK_MODE_ENV)
    {
        switch (comfort)
        {
        case DESK_COMFORT_OK:
            mask = DESK_LED_GREEN;
            break;
        case DESK_COMFORT_HOT:
            mask = DESK_LED_RED;
            break;
        case DESK_COMFORT_COLD:
            mask = DESK_LED_BLUE;
            break;
        case DESK_COMFORT_DRY:
        case DESK_COMFORT_HUMID:
            mask = DESK_LED_YELLOW;
            break;
        default:
            mask = ((now / desk_ticks(500U)) & 1U) ? DESK_LED_ALL : 0U;
            break;
        }
    }
    else if (current_mode == DESK_MODE_POMO)
    {
        if (current_phase == DESK_PHASE_FOCUS)
        {
            rt_uint32_t used = (focus_s > remain_s) ? (focus_s - remain_s) : 0U;
            rt_uint32_t lit = 1U + (used * 3U) / focus_s;

            if (lit > 4U) { lit = 4U; }
            mask = (rt_uint8_t)((1U << lit) - 1U);
        }
        else if (current_phase == DESK_PHASE_PAUSED)
        {
            mask = ((now / desk_ticks(500U)) & 1U) ? DESK_LED_YELLOW : 0U;
        }
        else if (current_phase == DESK_PHASE_BREAK)
        {
            mask = DESK_LED_BLUE;
        }
        else if (current_phase == DESK_PHASE_LONG_BREAK)
        {
            mask = ((now / desk_ticks(500U)) & 1U) ? DESK_LED_BLUE : 0U;
        }
    }
    else
    {
        if (sit_alert)
        {
            mask = ((now / desk_ticks(500U)) & 1U) ? DESK_LED_RED : 0U;
        }
        else if (sit_s >= sit_warn_s)
        {
            mask = DESK_LED_YELLOW;
        }
        else
        {
            mask = DESK_LED_GREEN;
        }
    }

    if (mask != last_led_mask)
    {
        board_led_set_mask(mask);
        last_led_mask = mask;
    }
}

static void enter_mode(desk_mode_t mode, rt_tick_t now)
{
    beep_stop();
    current_mode = mode;
    set_mode_flash(now);

    if (mode == DESK_MODE_POMO)
    {
        focus_done = 0;
        reset_pomodoro(now);
    }
    else if (mode == DESK_MODE_SIT)
    {
        last_motion_tick = now;
        sit_s = 0;
        sit_alert = 0;
        have_accel = 0;
    }
}

int svc_desk_init(void)
{
    if (rt_mutex_init(&desk_lock, "desk", RT_IPC_FLAG_PRIO) != RT_EOK)
    {
        return APP_ERR;
    }

    focus_s = DESK_FOCUS_S;
    break_s = DESK_BREAK_S;
    long_s = DESK_LONG_S;
    sit_warn_s = DESK_SIT_WARN_S;
    sit_alert_s = DESK_SIT_ALERT_S;
    current_mode = DESK_MODE_ENV;
    current_phase = DESK_PHASE_IDLE;
    remain_s = focus_s;
    comfort = DESK_COMFORT_UNKNOWN;
    /* Safe default: the desk companion never starts beeping on boot. */
    muted = 1;
    last_motion_tick = rt_tick_get();
    render_leds(rt_tick_get());
    return APP_OK;
}

void svc_desk_tick(rt_tick_t now)
{
    rt_mutex_take(&desk_lock, RT_WAITING_FOREVER);

    if ((current_mode == DESK_MODE_POMO) && running)
    {
        if ((rt_int32_t)(now - phase_end_tick) >= 0)
        {
            finish_pomodoro_phase(now);
        }
        else
        {
            remain_s = (rt_uint16_t)((phase_end_tick - now +
                                      desk_ticks(1000U) - 1U) /
                                     desk_ticks(1000U));
        }
    }

    if (current_mode == DESK_MODE_SIT)
    {
        sit_s = (rt_uint16_t)((now - last_motion_tick) / desk_ticks(1000U));

        if (!sit_alert && (sit_s >= sit_alert_s))
        {
            /* Alarm once per inactivity episode; LED stays red until reset. */
            sit_alert = 1;
            beep_start(beep_alert_pat);
        }
    }

    beep_tick(now);
    render_leds(now);
    rt_mutex_release(&desk_lock);
}

void svc_desk_update_sensor(const app_sensor_sample_t *sample, rt_tick_t now)
{
    rt_mutex_take(&desk_lock, RT_WAITING_FOREVER);
    update_comfort(sample);
    update_motion(sample, now);
    rt_mutex_release(&desk_lock);
}

void svc_desk_on_key(rt_uint8_t id, rt_uint8_t pressed)
{
    rt_tick_t now;

    if (!pressed)
    {
        return;
    }

    rt_mutex_take(&desk_lock, RT_WAITING_FOREVER);
    now = rt_tick_get();

    if (id == 0U)
    {
        if (current_mode == DESK_MODE_POMO)
        {
            if (running)
            {
                pause_pomodoro(now);
            }
            else
            {
                start_pomodoro(now);
            }
        }
        else if (current_mode == DESK_MODE_SIT)
        {
            beep_stop();
            last_motion_tick = now;
            sit_alert = 0;
        }
    }
    else if (id == 1U)
    {
        if (current_mode == DESK_MODE_POMO)
        {
            beep_stop();
            reset_pomodoro(now);
        }
        else if (current_mode == DESK_MODE_SIT)
        {
            beep_stop();
            last_motion_tick = now;
            sit_alert = 0;
        }
    }
    else if (id == 2U)
    {
        enter_mode((desk_mode_t)((current_mode + 1U) % 3U), now);
    }
    else if (id == 3U)
    {
        muted = muted ? 0U : 1U;
        if (muted)
        {
            beep_stop();
        }
        else
        {
            beep_start(beep_short_pat);
        }
    }

    rt_mutex_release(&desk_lock);
}

void svc_desk_fill_sample(app_sensor_sample_t *sample)
{
    if (sample == RT_NULL)
    {
        return;
    }

    rt_mutex_take(&desk_lock, RT_WAITING_FOREVER);
    sample->desk_mode = (rt_uint8_t)current_mode;
    sample->desk_phase = (rt_uint8_t)current_phase;
    sample->desk_remain = remain_s;
    sample->desk_cycles = focus_done;
    sample->desk_sit = sit_s;
    sample->desk_alert = sit_alert;
    sample->desk_mute = muted;
    sample->desk_comfort = comfort;
    rt_mutex_release(&desk_lock);
}

void svc_desk_get_status(desk_status_t *status)
{
    if (status == RT_NULL)
    {
        return;
    }

    rt_mutex_take(&desk_lock, RT_WAITING_FOREVER);
    status->mode = (rt_uint8_t)current_mode;
    status->phase = (rt_uint8_t)current_phase;
    status->running = running;
    status->mute = muted;
    status->focus_done = focus_done;
    status->sit_alert = sit_alert;
    status->remain_s = remain_s;
    status->sit_s = sit_s;
    status->comfort = comfort;
    rt_mutex_release(&desk_lock);
}

desk_mode_t svc_desk_mode(void)
{
    desk_mode_t mode;

    rt_mutex_take(&desk_lock, RT_WAITING_FOREVER);
    mode = current_mode;
    rt_mutex_release(&desk_lock);
    return mode;
}

int svc_desk_command(const char *action, int value)
{
    rt_tick_t now;

    if (action == RT_NULL)
    {
        return APP_ERR_PARAM;
    }

    rt_mutex_take(&desk_lock, RT_WAITING_FOREVER);
    now = rt_tick_get();

    if (rt_strcmp(action, "mode") == 0)
    {
        if ((value < 0) || (value > 2))
        {
            rt_mutex_release(&desk_lock);
            return APP_ERR_PARAM;
        }
        enter_mode((desk_mode_t)value, now);
    }
    else if (rt_strcmp(action, "start") == 0)
    {
        if (current_mode != DESK_MODE_POMO)
        {
            enter_mode(DESK_MODE_POMO, now);
        }
        start_pomodoro(now);
    }
    else if (rt_strcmp(action, "pause") == 0)
    {
        pause_pomodoro(now);
    }
    else if (rt_strcmp(action, "reset") == 0)
    {
        beep_stop();
        if (current_mode == DESK_MODE_POMO)
        {
            reset_pomodoro(now);
        }
        else if (current_mode == DESK_MODE_SIT)
        {
            last_motion_tick = now;
            sit_alert = 0;
        }
    }
    else if (rt_strcmp(action, "mute") == 0)
    {
        muted = value ? 1U : 0U;
        if (muted)
        {
            beep_stop();
        }
    }
    else if (rt_strcmp(action, "ack") == 0)
    {
        beep_stop();
        last_motion_tick = now;
        sit_alert = 0;
    }
    else if (rt_strcmp(action, "beep") == 0)
    {
        beep_start(beep_short_pat);
    }
    else
    {
        rt_mutex_release(&desk_lock);
        return APP_ERR_PARAM;
    }

    rt_mutex_release(&desk_lock);
    return APP_OK;
}

void svc_desk_beep(void)
{
    rt_mutex_take(&desk_lock, RT_WAITING_FOREVER);
    beep_start(beep_short_pat);
    rt_mutex_release(&desk_lock);
}

void svc_desk_set_test(rt_uint16_t focus_seconds, rt_uint16_t break_seconds,
                       rt_uint16_t sit_seconds)
{
    rt_tick_t now = rt_tick_get();

    rt_mutex_take(&desk_lock, RT_WAITING_FOREVER);

    if (focus_seconds < 2U)
    {
        focus_seconds = 2U;
    }
    if (break_seconds < 2U)
    {
        break_seconds = 2U;
    }
    if (sit_seconds < 4U)
    {
        sit_seconds = 4U;
    }

    focus_s = focus_seconds;
    break_s = break_seconds;
    long_s = break_seconds;
    sit_warn_s = (rt_uint16_t)((sit_seconds * 2U) / 3U);
    sit_alert_s = sit_seconds;
    focus_done = 0;
    beep_stop();
    reset_pomodoro(now);
    current_mode = DESK_MODE_POMO;
    last_motion_tick = now;
    sit_s = 0;
    sit_alert = 0;
    have_accel = 0;
    set_mode_flash(now);
    rt_mutex_release(&desk_lock);
}
