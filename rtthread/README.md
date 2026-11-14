# RT-Thread port for kyLin-V3.1

This directory is part of the `firmware/` Git repository. It contains the
RT-Thread BSP for the kyLin-V3.1 board (STM32F103RET6, 512 KB Flash / 64 KB RAM).

## Current status

- **Step 1 complete**: RT-Thread v5.3.0 boots, 72 MHz, USART1 msh at 115200.
- **Verified on board**:
  - RT-Thread banner + `msh />`
  - `ps` (thread list)
  - `free`
  - `list device`
- **Size**: 76,080 B ROM / 4,528 B RAM (linker reports 60 KB RAM region).
- **Clock**: HSE 12 MHz ×6 = 72 MHz.
- **Console**: USART1, PA9/PA10, 115200 8N1.

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

## Next steps

- Step 2: LED / KEY / IWDG / FAL + EasyFlash
- Step 3: software I2C + AT24C02 + SHT20 + ADC light + buzzer
- Step 4: ESP8266 AT Socket + Wi-Fi/TCP
- Step 5: application protocol + reconnect/stability
