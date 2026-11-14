#include <rtthread.h>
#include <rtdevice.h>
#include "app_config.h"
#include "app_types.h"
#include "board_key.h"

static const rt_base_t key_pins[BOARD_KEY_MAX] =
{
    APP_KEY1_PIN,
    APP_KEY2_PIN,
    APP_KEY3_PIN,
    APP_KEY4_PIN,
};

static rt_uint8_t stable_mask;
static rt_uint8_t debounce_count[BOARD_KEY_MAX];

rt_uint8_t board_key_read_mask(void)
{
    rt_uint8_t mask = 0;
    int i;

    for (i = 0; i < BOARD_KEY_MAX; i++)
    {
        /* Pull-up input: pressed == low. */
        if (rt_pin_read(key_pins[i]) == PIN_LOW)
        {
            mask |= (1UL << i);
        }
    }

    return mask;
}

int board_key_init(void)
{
    int i;

    for (i = 0; i < BOARD_KEY_MAX; i++)
    {
        rt_pin_mode(key_pins[i], PIN_MODE_INPUT_PULLUP);
        debounce_count[i] = 0;
    }

    stable_mask = board_key_read_mask();
    return APP_OK;
}

rt_uint8_t board_key_scan(void)
{
    rt_uint8_t raw = board_key_read_mask();
    int i;

    for (i = 0; i < BOARD_KEY_MAX; i++)
    {
        rt_uint8_t bit = (1UL << i);

        if ((raw & bit) != (stable_mask & bit))
        {
            debounce_count[i]++;
            if (debounce_count[i] >= APP_KEY_DEBOUNCE_COUNT)
            {
                stable_mask ^= bit;
                debounce_count[i] = 0;
            }
        }
        else
        {
            debounce_count[i] = 0;
        }
    }

    return stable_mask;
}

int board_key_is_pressed(board_key_id_t id)
{
    if (id >= BOARD_KEY_MAX)
    {
        return 0;
    }

    return (stable_mask & (1UL << id)) != 0;
}
