/* Minimal bootloader for kyLin-V3.1.
 *
 * Step 1: validate the application vector table and jump to 0x08004000.
 * Ymodem/flash copy will be added in the next step.
 */

#include "stm32f10x.h"

#define BOOT_APP_BASE       0x08004000U
#define BOOT_APP_SIZE       (232U * 1024U)
#define BOOT_LED_PORT       GPIOB
#define BOOT_LED_PIN        GPIO_Pin_6

static void boot_delay(volatile uint32_t count)
{
    while (count-- != 0U)
    {
    }
}

static void boot_uart_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_USART1, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    uart.USART_BaudRate = 115200;
    uart.USART_WordLength = USART_WordLength_8b;
    uart.USART_StopBits = USART_StopBits_1;
    uart.USART_Parity = USART_Parity_No;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &uart);
    USART_Cmd(USART1, ENABLE);
}

static void boot_led_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = BOOT_LED_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(BOOT_LED_PORT, &gpio);
    GPIO_SetBits(BOOT_LED_PORT, BOOT_LED_PIN); /* LED off (active low). */
}

static void boot_uart_putc(char ch)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
    {
    }
    USART_SendData(USART1, (uint16_t)(uint8_t)ch);
}

static void boot_uart_puts(const char *text)
{
    while (*text != '\0')
    {
        boot_uart_putc(*text++);
    }
}

static void boot_uart_hex32(uint32_t value)
{
    static const char hex[] = "0123456789abcdef";
    int shift;

    boot_uart_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4)
    {
        boot_uart_putc(hex[(value >> (uint32_t)shift) & 0xFU]);
    }
}

static int boot_app_valid(uint32_t *stack, uint32_t *entry)
{
    uint32_t sp = *(volatile uint32_t *)BOOT_APP_BASE;
    uint32_t pc = *(volatile uint32_t *)(BOOT_APP_BASE + 4U);

    *stack = sp;
    *entry = pc;

    if ((sp < 0x20000000U) || (sp > 0x20010000U))
    {
        return 0;
    }
    if ((pc < BOOT_APP_BASE) || (pc >= (BOOT_APP_BASE + BOOT_APP_SIZE)))
    {
        return 0;
    }
    return 1;
}

static void boot_jump(uint32_t stack, uint32_t entry)
{
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    USART_Cmd(USART1, DISABLE);
    SCB->VTOR = BOOT_APP_BASE;
    __set_MSP(stack);
    ((void (*)(void))entry)();

    while (1)
    {
    }
}

/* Keep the bootloader independent of newlib. */
void __libc_init_array(void)
{
}

void *memcpy(void *dst, const void *src, unsigned int len)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    while (len-- != 0U)
    {
        *d++ = *s++;
    }
    return dst;
}

void *memset(void *dst, int value, unsigned int len)
{
    unsigned char *d = (unsigned char *)dst;

    while (len-- != 0U)
    {
        *d++ = (unsigned char)value;
    }
    return dst;
}

int main(void)
{
    uint32_t stack = 0;
    uint32_t entry = 0;

    boot_uart_init();
    boot_led_init();

    boot_uart_puts("\r\n[boot] kyLin-V3.1 bootloader\r\n");

    if (boot_app_valid(&stack, &entry))
    {
        boot_uart_puts("[boot] app SP=");
        boot_uart_hex32(stack);
        boot_uart_puts(" entry=");
        boot_uart_hex32(entry);
        boot_uart_puts("\r\n[boot] jump\r\n");
        boot_delay(100000U);
        boot_jump(stack, entry);
    }

    boot_uart_puts("[boot] no valid app, stay in bootloader\r\n");
    while (1)
    {
        GPIO_ResetBits(BOOT_LED_PORT, BOOT_LED_PIN);
        boot_delay(400000U);
        GPIO_SetBits(BOOT_LED_PORT, BOOT_LED_PIN);
        boot_delay(400000U);
    }
}
