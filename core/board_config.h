/**
 * board_config.h -- board-specific compile-time configuration.
 *
 * Target board: STM32F103RET6 (LQFP64, 512 KB Flash, 64 KB RAM).
 *
 * IMPORTANT: set BOARD_HSE_HZ to the crystal actually fitted on the board.
 *   - 12 MHz -> PLL x6 = 72 MHz (confirmed on the current F103RET6 board)
 *   - 8 MHz  -> PLL x9 = 72 MHz (common on other F103RE boards)
 *
 * A wrong value here changes the PLL output and therefore the UART baud rate.
 * With 8 MHz assumed on a 12 MHz board the core would run at 108 MHz.
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#define BOARD_HSE_HZ   12000000U
#define BOARD_MCU_NAME "STM32F103RET6"

#endif /* BOARD_CONFIG_H */
