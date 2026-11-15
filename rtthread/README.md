# RT-Thread port for kyLin-V3.1

This directory is part of the `firmware/` Git repository. It contains the
RT-Thread BSP for the kyLin-V3.1 board (STM32F103RET6, 512 KB Flash / 64 KB RAM).

## Current status

- **Step 1 complete**: RT-Thread v5.3.0 boots, 72 MHz, USART1 msh at 115200.
- **Step 2 complete**: application skeleton, LED/KEY/buzzer, IWDG, FAL + EasyFlash config, msh commands.
- **Step 3 complete**: software I2C (swi2c1), AT24C02, SHT20, PC3 ADC light, sensor task.
- **Step 3b complete**: ADXL362 on SPI1 (PA4 CS/PA5 SCK/PA6 MISO/PA7 MOSI),
  2 g / 100 Hz, INT1/INT2 data-ready IRQ, FIFO batch read, zero-offset calibration,
  acceleration + `valid` included in telemetry.
- **Step 4 complete**: ESP8266 AT Socket, Wi-Fi join, DHCP, DNS, TCP client.
- **Step 5 complete**: NDJSON uplink, command downlink, local dashboard (`server/server.py`).
- **Step 5 verified on board**: live temperature/humidity/light frames reach the PC;
  LED/beep/period commands are applied. The HTTP dashboard uses keep-alive to
  avoid browser polling socket exhaustion.
- **Step 5 stability**: reconnect uses exponential backoff (1–30 s), stale
  queued samples are dropped after reconnect, and `net stat` exposes link,
  connect, TX/RX and error counters.
- **Step 6 OTA**: 16 KB bootloader, 232 KB app slot, 232 KB download slot,
  Ymodem-1K + CRC32 image validation; `ota` / `ota status` verified on board.
- **Size**: app 225,780 B ROM / 13,220 B RAM; bootloader 4,764 B / 16 KB.
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
> so the same task can transmit without being blocked by `recv()`. Reconnect
> backoff is capped at 30 s, and the message queue is drained while offline so
> reconnecting does not replay a burst of stale samples.

## Layout

```text
firmware/rtthread/
├─ README.md
├─ setup.sh                  # fetch RT-Thread, STM32 HAL/CMSIS, EasyFlash, at_device
├─ build.sh                  # WSL build wrapper
├─ docs/
│  ├─ architecture.md
│  ├─ adxl362.md
│  └─ verification.md
├─ ota_layout.h              # shared flash layout / OTA container format
├─ bootloader/               # one-time OTA bootloader (bare-metal)
├─ server/
│  ├─ server.py              # TCP collector + HTTP dashboard
│  └─ soak.ps1               # optional long-run API logger
├─ tools/
│  ├─ verify-rtthread.ps1    # build / flash / serial / API gate
│  ├─ ota_pack.py            # package rtthread.bin as .ota
│  ├─ ota_send.ps1           # Ymodem sender
│  └─ verify-ota.ps1         # end-to-end OTA gate
└─ bsp/stm32/
   └─ stm32f103-kylin-v31/   # BSP (board, applications, Kconfig, rtconfig)
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

`build.sh` also builds `bootloader/build/bootloader.hex` and `.bin`. The app
starts at `0x08004000`; the bootloader at `0x08000000` is required for the app
to boot after the OTA layout change.

## Flash

From Windows, flash the bootloader first and then the app:

```powershell
cd H:\RTOS\OneNet_Mini\firmware
.\flash.bat "rtthread\bootloader\build\bootloader.hex"
.\flash.bat "rtthread\bsp\stm32\stm32f103-kylin-v31\rt-thread.hex"
```

The one-command gate below does both automatically.

Then open the USART1 debug port (CH340, 115200 8N1). Expected:

```text
 \ | /
- RT -     Thread Operating System
 / | \     5.3.0 build ...
 2006 - 2024 Copyright by RT-Thread team
msh >
```

## Verify build + board + dashboard

From `firmware/rtthread/`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-rtthread.ps1 -Port COM3
```

The gate builds, restores the image size, flashes, resets, captures the RT-Thread
banner / `[app] started` / `[sensor]`, then checks `/api/data` for fresh board
telemetry. It returns `RESULT: PASS` only when every enabled stage passes and
writes logs to `rtthread/logs/rtthread-verify-<timestamp>/`.

See [`docs/verification.md`](docs/verification.md) for parameters, evidence
files, and troubleshooting. Use `-SkipFlash -SkipSerial -SkipApi` for a
build-only check, or `-SkipApi` when the PC dashboard is not running.

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
{"v":1,"t":"d","seq":1,"up":123,"temp":28.6,"humi":71.4,"light":1593,
 "ax":0,"ay":-3,"az":1001,"valid":7}
```

`ax`/`ay`/`az` are ADXL362 acceleration in mg. `valid` bit0/1/2 marks
SHT20 / light / ADXL362 validity.

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

Check live link and counters on the board:

```text
msh > net stat
```

The output includes link/socket state, current reconnect backoff, connect
attempts/successes, disconnect count, TX frames/errors/dropped samples, and
RX packet/command/error counts.

Run a soak test on the PC while the board and server stay up:

```powershell
cd H:\RTOS\OneNet_Mini\firmware\rtthread
powershell -NoProfile -ExecutionPolicy Bypass -File .\server\soak.ps1 -Hours 24
```

The CSV is written under `%TEMP%\kylin-soak-*.csv` unless `-OutFile` is given.
It records API availability, board online state, last sequence/uptime, and data
age every 10 seconds.

Windows firewall must allow inbound TCP 9000 for Python on the active network
profile. The verified setup uses the **Private** profile. If the board can
connect but no frames arrive, temporarily allow Python or add a rule:

```powershell
New-NetFirewallRule -DisplayName "Kylin TCP 9000" -Direction Inbound `
  -Profile Any -Action Allow -Protocol TCP -LocalPort 9000
```

Then reset the board. The page shows temperature, humidity, light, acceleration,
sequence, uptime, history, and buttons for LED/beep/upload-period.

## ADXL362

The board's ADXL362 is on SPI1:

```text
PA4 CS, PA5 SCK, PA6 MISO, PA7 MOSI
INT1 = PB0, INT2 = PA8 (both data-ready interrupts)
```

The driver uses direct SPI1 registers (no extra RT-Thread SPI HAL code) and
configures 2 g range, 100 Hz ODR, mode 3, 4.5 MHz.

```text
msh > adxl id
adxl: devid=0xad partid=0xf2

msh > adxl read
adxl: X=0 mg Y=-3 mg Z=1001 mg

msh > adxl cal
adxl: calibrated offsets X=51 Y=105 Z=154 mg

msh > adxl irq
adxl: INT1 data-ready count=32, INT2 data-ready count=32

msh > adxl fifo
adxl: fifo samples=16 first=48/103/1159 last=49/107/1155 mg
```

`adxl cal` assumes the board is level; offsets are RAM-only and cleared by
`adxl reset` or a reboot. INT1 (PB0) and INT2 (PA8) are both wired for
data-ready; FIFO batch reads are polled by `adxl fifo`. The sensor task includes acceleration and validity in
the NDJSON uplink; the dashboard shows Pitch/Roll computed on the PC. See
[`docs/adxl362.md`](docs/adxl362.md).

## OTA over Ymodem

The app now lives at `0x08004000`. A small bootloader at `0x08000000` owns the
Ymodem receiver and copies a validated image into the app slot. EasyFlash stays
at `0x08078000`, so Wi-Fi/server configuration is preserved.

Start an update:

```text
msh > ota
```

The board reboots to the bootloader. Package and send from the PC:

```powershell
cd H:\RTOS\OneNet_Mini\firmware\rtthread

python .\tools\ota_pack.py .\bsp\stm32\stm32f103-kylin-v31\rtthread.bin `
    -o .\rtthread.ota --version 0x1234

powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ota_send.ps1 `
    -Port COM3 -Image .\rtthread.ota
```

The bootloader validates the container magic, image size, header CRC32 and image
CRC32, copies the payload, verifies it again, then boots the new app. Check the
stored version:

```text
msh > ota status
```

End-to-end automated gate:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify-ota.ps1 -Port COM3
```

See [`docs/ota.md`](docs/ota.md) for the partition table, metadata format, failure
handling, and recovery. Hold KEY1 (PC13, active low) while resetting to force the
bootloader for recovery.

## Next steps

- Optional: reconnect backoff tuning and 24 h stability test.
- Optional: persist dashboard history across server restarts.

