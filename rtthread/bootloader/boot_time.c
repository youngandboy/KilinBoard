#include "stm32f10x.h"
#include "boot_time.h"

static volatile uint32_t boot_ticks;

void SysTick_Handler(void)
{
    boot_ticks++;
}

void boot_time_init(void)
{
    (void)SysTick_Config(SystemCoreClock / 1000U);
}

uint32_t boot_millis(void)
{
    return boot_ticks;
}
