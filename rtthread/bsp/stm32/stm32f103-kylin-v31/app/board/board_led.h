#ifndef BOARD_LED_H__
#define BOARD_LED_H__

#include <rtthread.h>

typedef enum
{
    BOARD_LED_RED = 0,
    BOARD_LED_GREEN,
    BOARD_LED_YELLOW,
    BOARD_LED_BLUE,
    BOARD_LED_MAX
} board_led_id_t;

int board_led_init(void);
void board_led_write(board_led_id_t id, int on);
void board_led_toggle(board_led_id_t id);
void board_led_set_mask(rt_uint8_t mask);
rt_uint8_t board_led_get_mask(void);

#endif /* BOARD_LED_H__ */
