#include <rtthread.h>
#include <rtdevice.h>
#include "app_config.h"
#include "app_types.h"
#include "board_led.h"

static const rt_base_t led_pins[BOARD_LED_MAX] =
{
    APP_LED_RED_PIN,
    APP_LED_GREEN_PIN,
    APP_LED_YELLOW_PIN,
    APP_LED_BLUE_PIN,
};

static rt_uint8_t led_mask;

static void led_write_pin(board_led_id_t id, int on)
{
    /* kyLin-V3.1 LEDs are active-low. */
    rt_pin_write(led_pins[id], on ? PIN_LOW : PIN_HIGH);
}

int board_led_init(void)
{
    int i;

    for (i = 0; i < BOARD_LED_MAX; i++)
    {
        rt_pin_mode(led_pins[i], PIN_MODE_OUTPUT);
        led_write_pin((board_led_id_t)i, 0);
    }

    led_mask = 0;
    return APP_OK;
}

void board_led_write(board_led_id_t id, int on)
{
    if (id >= BOARD_LED_MAX)
    {
        return;
    }

    led_write_pin(id, on);
    if (on)
    {
        led_mask |= (1UL << id);
    }
    else
    {
        led_mask &= ~(1UL << id);
    }
}

void board_led_toggle(board_led_id_t id)
{
    if (id >= BOARD_LED_MAX)
    {
        return;
    }

    board_led_write(id, !(led_mask & (1UL << id)));
}

void board_led_set_mask(rt_uint8_t mask)
{
    int i;

    for (i = 0; i < BOARD_LED_MAX; i++)
    {
        led_write_pin((board_led_id_t)i, (mask & (1UL << i)) != 0);
    }

    led_mask = mask & ((1UL << BOARD_LED_MAX) - 1);
}

rt_uint8_t board_led_get_mask(void)
{
    return led_mask;
}
