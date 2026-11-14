#include <rtthread.h>
#include <stdlib.h>
#include "board_led.h"
#include "board_key.h"
#include "board_buzzer.h"
#include "task_supervisor.h"

static void led_usage(void)
{
    rt_kprintf("usage: led <0-3> <on|off|toggle>\n");
}

static int cmd_led(int argc, char **argv)
{
    int id;
    board_led_id_t led;

    if (argc != 3)
    {
        led_usage();
        return -1;
    }

    id = atoi(argv[1]);
    if ((id < 0) || (id >= BOARD_LED_MAX))
    {
        led_usage();
        return -1;
    }
    led = (board_led_id_t)id;

    if (rt_strcmp(argv[2], "on") == 0)
    {
        board_led_write(led, 1);
    }
    else if (rt_strcmp(argv[2], "off") == 0)
    {
        board_led_write(led, 0);
    }
    else if (rt_strcmp(argv[2], "toggle") == 0)
    {
        board_led_toggle(led);
    }
    else
    {
        led_usage();
        return -1;
    }

    rt_kprintf("led%d mask=0x%x\n", id + 1, board_led_get_mask());
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_led, led, control LED: led <0-3> <on|off|toggle>);

static int cmd_key(int argc, char **argv)
{
    int i;

    (void)argc;
    (void)argv;

    rt_kprintf("key raw=0x%x stable=0x%x\n",
               board_key_read_mask(), board_key_scan());

    for (i = 0; i < BOARD_KEY_MAX; i++)
    {
        rt_kprintf("  key%d: %s\n", i + 1,
                   board_key_is_pressed((board_key_id_t)i) ? "down" : "up");
    }

    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_key, key, show key state);

static int cmd_beep(int argc, char **argv)
{
    int ms;

    if (argc != 2)
    {
        rt_kprintf("usage: beep <ms>\n");
        return -1;
    }

    ms = atoi(argv[1]);
    if ((ms <= 0) || (ms > 5000))
    {
        rt_kprintf("beep: ms out of range (1..5000)\n");
        return -1;
    }

    board_buzzer_beep((rt_uint32_t)ms);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_beep, beep, beep for N ms: beep <ms>);

static int cmd_wdt(int argc, char **argv)
{
    if (argc != 2 || rt_strcmp(argv[1], "test") != 0)
    {
        rt_kprintf("usage: wdt test   (stop feeding IWDG, expect reset in ~5 s)\n");
        return -1;
    }

    task_supervisor_set_feed_enabled(0);
    rt_kprintf("wdt: feed disabled, reset expected in ~5 s\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_wdt, wdt, watchdog test: wdt test);
