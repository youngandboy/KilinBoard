#ifndef BOARD_KEY_H__
#define BOARD_KEY_H__

#include <rtthread.h>

typedef enum
{
    BOARD_KEY_1 = 0,
    BOARD_KEY_2,
    BOARD_KEY_3,
    BOARD_KEY_4,
    BOARD_KEY_MAX
} board_key_id_t;

int board_key_init(void);
rt_uint8_t board_key_read_mask(void);
rt_uint8_t board_key_scan(void);
int board_key_is_pressed(board_key_id_t id);

#endif /* BOARD_KEY_H__ */
