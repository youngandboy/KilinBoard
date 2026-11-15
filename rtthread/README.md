# RT-Thread port for kyLin-V3.1

This directory is part of the `firmware/` Git repository. It contains the
RT-Thread BSP for the kyLin-V3.1 board (STM32F103RET6, 512 KB Flash / 64 KB RAM).

## Current status

- **Step 1 complete**: RT-Thread v5.3.0 boots, 72 MHz, USART1 msh at 115200.
- **Step 2 complete**: application skeleton, LED/KEY/buzzer, IWDG, FAL + EasyFlash config, msh commands.
- **Step 3 complete**: software I2C (swi2c1), AT24C02, SHT20, PC3 ADC light, sensor task.
- **Step 4 complete**: ESP8266 AT Socket, Wi-Fi join, DHCP, DNS, TCP client.
- **Step 5 complete**: NDJSON uplink, command downlink, local dashboard (`server/server.py`).
- **Step 5 verified on board**: live temperature/humidity/light frames reach the PC;
  LED/beep/period commands are applied. The HTTP dashboard uses keep-alive to
  avoid browser polling socket exhaustion.
- **Size**: 219,476 B ROM / 12,952 B RAM.
- **Clock**: HSE 12 MHz x6 = 72 MHz.
- **Console**: USART1, PA9/PA10, 115200 8N1.

> Step 2 fix: `IDLE_THREAD_STACK_SIZE` increased from 256 to 512; with EasyFlash/ulog
> the idle thread overflowed and triggered `thread:tidle0 stack overflow`.
> Step 3 fix: SHT20 uses no-hold commands 0xF3/0xF5 with 100/50 ms wait; the
> bit-banged I2C does not support hold-master clock stretching. ADC1 MSP enables
> the clock and configures PC3 as analog.
> Step 4 fix: AT 1.3.x `AT+CIPSTART` treats the 5th argument as local port, not
> keepalive; `patches/at_device_esp8266_at130.patch` removes `,60` for old AT.
> `AT+CIPDNS?` is unsupported; DNS still works through `AT+CIPDOMAIN`.
> Step 5 fix: RT-Thread `rt_mq_recv()` returns the received message length, not
> `RT_EOK`; the net task must test `> 0`. Downlink polling uses `MSG_DONTWAIT`
> so the same task can transmit without being blocked by `recv()`.

## Layout

```text
firmware/rtthread/
鈹溾攢 README.md
鈹溾攢 setup.sh                 # fetch RT-Thread, STM32 HAL/CMSIS, EasyFlash, at_device
鈹溾攢 build.sh                 # WSL build wrapper
鈹斺攢 bsp/stm32/
   鈹斺攢 stm32f103-kylin-v31/  # BSP (board, applications, Kconfig, rtconfig)
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

## Step 5 protocol and local dashboard

The board connects to the PC as a TCP client; the PC runs a local TCP collector
and an HTTP dashboard.

```text
Board (esp0) -- TCP client --> PC 192.168.3.103:9000
Browser -> http://localhost:8080
```

Uplink (NDJSON, one JSON object per line):

```json
{"v":1,"t":"d","seq":1,"up":123,"temp":28.6,"humi":71.4,"light":1593}
```

Downlink commands:

```json
{"t":"c","led":0,"on":1}
{"t":"c","beep":100}
{"t":"c","period":5}
```

Run the dashboard on the PC:

```powershell
cd H:\RTOS\OneNet_Mini\firmware\rtthread\server
python server.py
```

Configure the board target (EasyFlash, not committed):

```text
msh > cfg set srv_host 192.168.3.103
msh > cfg set srv_port 9000
msh > cfg save
```

Windows firewall must allow inbound TCP 9000 for Python on the active network
profile. The verified setup uses the **Private** profile. If the board can
connect but no frames arrive, temporarily allow Python or add a rule:

```powershell
New-NetFirewallRule -DisplayName "Kylin TCP 9000" -Direction Inbound `
  -Profile Any -Action Allow -Protocol TCP -LocalPort 9000
```

Then reset the board. The page shows temperature, humidity, light, sequence,
uptime, history, and buttons for LED/beep/upload-period.

## Next steps

- Optional: reconnect backoff tuning and 24 h stability test.
- Optional: persist dashboard history across server restarts.

