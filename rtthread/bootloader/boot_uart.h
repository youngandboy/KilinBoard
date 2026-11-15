#ifndef BOOT_UART_H__
#define BOOT_UART_H__

#include <stdint.h>

void boot_uart_init(void);
void boot_uart_putc(char ch);
void boot_uart_puts(const char *text);
int boot_uart_getc(uint32_t timeout_ms);

#endif /* BOOT_UART_H__ */
