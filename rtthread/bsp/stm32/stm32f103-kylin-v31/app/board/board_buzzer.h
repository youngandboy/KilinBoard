#ifndef BOARD_BUZZER_H__
#define BOARD_BUZZER_H__

#include <rtthread.h>

int board_buzzer_init(void);
void board_buzzer_on(void);
void board_buzzer_off(void);
void board_buzzer_beep(rt_uint32_t ms);

#endif /* BOARD_BUZZER_H__ */
