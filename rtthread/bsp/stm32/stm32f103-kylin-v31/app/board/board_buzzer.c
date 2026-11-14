#include <rtthread.h>
#include <rtdevice.h>
#include <stm32f1xx_hal.h>
#include "app_config.h"
#include "app_types.h"
#include "board_buzzer.h"

int board_buzzer_init(void)
{
    /* PB3 is JTDO. Disable JTAG but keep SWD. */
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    rt_pin_mode(APP_BUZZER_PIN, PIN_MODE_OUTPUT);
    board_buzzer_off();
    return APP_OK;
}

void board_buzzer_on(void)
{
    /* Active-high driver transistor. */
    rt_pin_write(APP_BUZZER_PIN, PIN_HIGH);
}

void board_buzzer_off(void)
{
    rt_pin_write(APP_BUZZER_PIN, PIN_LOW);
}

void board_buzzer_beep(rt_uint32_t ms)
{
    board_buzzer_on();
    rt_thread_mdelay(ms);
    board_buzzer_off();
}
