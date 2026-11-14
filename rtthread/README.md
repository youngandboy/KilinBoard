# RT-Thread port for kyLin-V3.1

This directory is part of the `firmware/` Git repository. It contains the
RT-Thread BSP for the kyLin-V3.1 board (STM32F103RET6, 512 KB Flash / 64 KB RAM).

## Current status

- **Step 1 complete**: RT-Thread v5.3.0 boots, 72 MHz, USART1 msh at 115200.
- **Step 2 complete**: application skeleton, LED/KEY/buzzer, IWDG, FAL + EasyFlash config, msh commands.
- **Step 3 complete**: software I2C (swi2c1), AT24C02, SHT20, PC3 ADC light, sensor task.
- **Step 3 verified on board**:
  - `i2c scan` -> 0x40 (SHT20), 0x50 (AT24C02)
  - `sensor` -> T=28.6 C, RH=71.4 %, light≈1.7 V
  - `at24 write/read` -> round-trip OK
  - periodic `[sensor]` log every 1 s
- **Size**: 123,916 B ROM / 8,776 B RAM.
- **Clock**: HSE 12 MHz ×6 = 72 MHz.
- **Console**: USART1, PA9/PA10, 115200 8N1.

> Step 2 fix: `IDLE_THREAD_STACK_SIZE` increased from 256 to 512; with EasyFlash/ulog
> the idle thread overflowed and triggered `thread:tidle0 stack overflow`.
> Step 3 fix: SHT20 uses no-hold commands 0xF3/0xF5 with 100/50 ms wait; the
> bit-banged I2C does not support hold-master clock stretching. ADC1 MSP enables
> the clock and configures PC3 as analog.

## Layout

```text
firmware/rtthread/
├─ README.md
├─ setup.sh                 # fetch RT-Thread + STM32 HAL/CMSIS packages
├─ build.sh                 # WSL build wrapper
└─ bsp/stm32/
   └─ stm32f103-kylin-v31/  # BSP (board, applications, Kconfig, rtconfig)
```

The RT-Thread kernel/components are intentionally **not** copied into this
repository. They live in an external RT-Thread v5.3.0 source tree and are
referenced through `RTT_ROOT`. The BSP only contains board-specific code.

## Prerequisites

- WSL Ubuntu with `arm-none-eabi-gcc` and `scons`
- RT-Thread **v5.3.0** source tree, default path:
  `/mnt/h/RTOS/rt-thread-5.3.0`
- ST-LINK CLI for flashing (`firmware/flash.bat`)

Install `scons` in WSL if needed:

```bash
python3 -m pip install --user --break-system-packages scons
```

## Setup

```bash
cd /mnt/h/RTOS/OneNet_Mini/firmware/rtthread
bash setup.sh
```

`setup.sh` clones RT-Thread v5.3.0 if missing and downloads the three required
STM32F1 packages into the BSP `packages/` directory (ignored by Git).

## Build

```bash
cd /mnt/h/RTOS/OneNet_Mini/firmware/rtthread
PATH="$HOME/.local/bin:$PATH" bash build.sh
```

Outputs in `bsp/stm32/stm32f103-kylin-v31/`:

```text
rt-thread.elf
rt-thread.hex
rtthread.bin
```

## Flash

From Windows:

```powershell
cd H:\RTOS\OneNet_Mini\firmware
.\flash.bat "rtthread\bsp\stm32\stm32f103-kylin-v31\rt-thread.hex"
```

Then open the USART1 debug port (CH340, 115200 8N1). Expected:

```text
 \ | /
- RT -     Thread Operating System
 / | \     5.3.0 build ...
 2006 - 2024 Copyright by RT-Thread team
msh >
```

## Notes

- `HSE_VALUE=12000000` is set in `rtconfig.py` **and** used by
  `SystemClock_Config()` in `board/board.c`. Both are required; otherwise the
  UART baud rate is wrong (observed 172800 instead of 115200).
- The BSP is derived from RT-Thread's `stm32f103-onenet-nbiot` BSP, which has
  the same MCU/Flash/RAM but a different board pinout. Only the board support
  code was adapted; the original NB-IoT application was removed.
- `packages/`, `build/`, `*.elf`, `*.hex`, `*.bin` are ignored by Git.

## ESP8266 probe (Step 4 pre-check)

USART2 is enabled and a small `esp` msh command resets the module on PA0 and
forwards one AT command per invocation.

```text
msh > esp reset
esp: reset on PA0
esp: ready

msh > esp AT+GMR
AT version:1.3.0.0(Jul 14 2016 18:54:01)
SDK version:2.0.0(5a875ba)
Farylink Technology Co., Ltd. v1.0.0.2
May 11 2017 22:23:58
OK
WIFI DISCONNECT
```

Current module firmware is **AT 1.3.0.0 / SDK 2.0.0 (2016)**. This is old;
`at_device` may need a newer ESP8266 AT firmware (1.7.x) for reliable
`AT+CIPSTART`/socket behavior. TCP-only operation should still work; upgrade is
recommended before Step 4 if the module has 1 MB Flash.

## Next steps

- Step 4: ESP8266 AT Socket + Wi-Fi/TCP
- Step 5: application protocol + reconnect/stability
