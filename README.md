# OneNet_Mini_Bare —— STM32F103 裸机起步工程

基于 `doc/1.OneNET-Mini-V1.x-基础学习例程/2.串口打印` 复制出来的独立工程，用来验证：

**上电 → 启动文件 → `main()` → USART1 打印 → 死循环**

当前不包含 RTOS，后续可以在这套工程上继续加外设、再加 UCOS/RT-Thread。
只使用 **ARM GCC** 命令行构建（Windows 原生或 WSL）。

---

## 1. 工程信息

| 项目 | 内容 |
| --- | --- |
| 构建 | `Makefile` + `build.bat` / `build.ps1`（ARM GCC） |
| 目标器件 | STM32F103RET6（LQFP64，512 KB Flash / 64 KB SRAM） |
| 系统时钟 | 72 MHz（外部 12 MHz 晶振，PLL ×6；若板载 8 MHz 改 `core/board_config.h`） |
| 调试串口 | USART1，PA9(TX) / PA10(RX)，115200 8N1 |
| 下载方式 | ST-Link，SWD |
| 入口文件 | `user/main.c` |
| 延时 | `hardware/src/delay.c`，SysTick 阻塞延时 |
| 串口打印 | `hardware/src/usart.c` 的 `UsartPrintf()` |

产物位置：

- ARM GCC：`output/gcc/OneNet_Mini_Bare.hex`

---

## 2. 编译方式

### 2.1 脚本编译（ARM GCC）

`build.bat` 会自动选择工具链：

1. 如果 Windows 的 `PATH` 里有 `arm-none-eabi-gcc`，直接在本机编译；
2. 否则如果装了 WSL，并且 WSL 里有 `arm-none-eabi-gcc` + `make`，自动转到 WSL 编译；
3. 两者都没有时给出安装提示。

常用命令（在 `firmware/` 目录或任意目录执行均可）：

```bat
build.bat
build.bat -Clean
build.bat -Release
build.bat -Clean -Release
```

- 默认：`-O0 -g3`，方便调试。
- `-Release`：`-Os -g0`，体积更小。
- `-Clean`：先删除 `build/` 和 `output/gcc/` 里的 GCC 产物。
- 如果工具链不在常见位置，可以显式指定：

```bat
build.bat -Toolchain "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin"
```

WSL 里安装工具链（本机已验证）：

```bash
sudo apt update
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi make
```

也可以直接在 WSL / Linux 里使用 Makefile：

```bash
cd /mnt/h/RTOS/OneNet_Mini/firmware
make            # 调试版
make RELEASE=1  # 发布版
make size
make clean
```

GCC 产物：

```
output/gcc/OneNet_Mini_Bare.elf
output/gcc/OneNet_Mini_Bare.hex
output/gcc/OneNet_Mini_Bare.bin
output/gcc/OneNet_Mini_Bare.map
```

### 2.2 ST-LINK 命令行烧录

脚本：`flash.bat`。默认烧 `output/gcc/OneNet_Mini_Bare.hex`。

```bat
flash.bat
flash.bat -Full
flash.bat "path\to\firmware.hex"
flash.bat -Full "path\to\firmware.hex"
```

- 使用 `ST-LINK_CLI.exe -c SWD -P ... -V while_programming -Rst -Run -Q -NoPrompt`
- 只走 SWD；板子的 4 pin SWD 接口不需要接 reset
- `-Full` 会先执行全片擦除
- 如果 **STM32 ST-LINK Utility GUI 开着**，探针会被独占，CLI 会报 `ST-LINK Already Used`；
  `flash.bat` 检测到 GUI 会直接退出，先关掉 GUI 再运行
- 如果 `ST-LINK_CLI.exe` 不在默认路径，设置环境变量 `STLINK_CLI` 指向它

---

## 3. 预期串口输出

上电后应看到类似内容，并且每秒增加一行：

```text
========================================
 OneNET Mini bare-metal project
 MCU  : STM32F103RET6, HSE 12MHz
 UART : USART1 PA9/PA10, 115200 8N1
 Build: Nov 12 2026 22:26:42
========================================
Hello from main()!

main loop is running, counter = 0
main loop is running, counter = 1
main loop is running, counter = 2
...
```

`counter` 每秒加 1，说明程序已经正常跑到 `main()` 并停留在主循环。

---

## 4. 已验证到什么程度

- 目标板已从 OneNET Mini（F103CB/C8）切换到 **STM32F103RET6**（512 KB / 64 KB）。
- 构建：`build.bat` / `make` 在 WSL 下用 `arm-none-eabi-gcc` 编译链接通过，无警告；
  使用 `startup/startup_stm32f103xe.s` + `linker/STM32F103RE.ld`。
- 真实板子烧录/串口：**尚未在新 F103RET6 板上验证**；烧录后应看到
  `MCU : STM32F103RET6, HSE 8MHz`（或 12MHz，取决于 `core/board_config.h`）。
- 旧的 OneNET Mini（F103C8）验证记录仍保留在 `docs/EVIDENCE/legacy_f103c8/`，仅作历史参考。

> **晶振已确认**：`core/board_config.h` 当前为 `BOARD_HSE_HZ = 12000000`（12 MHz，PLL ×6）。
> 如果换成 8 MHz 晶振的板子，改成 `8000000`；否则 PLL 会跑偏，串口也会乱码。
>
> **Flash/RAM 配置**：链接脚本为 `linker/STM32F103RE.ld`（512 KB / 64 KB）。
> 不要再使用 `STM32F103CB.ld`。

可选的 QEMU 验证命令：

```bash
cd firmware
make emu
```

`make emu` 会用 `linker/STM32F100_EMU.ld` 单独链接一个 8 KB RAM 的镜像，
只用于仿真，不要烧到真实板子。

---

## 5. 目录结构

```text
firmware/
├─ Makefile                   ARM GCC 构建
├─ build.bat                  GCC 构建入口（Windows / WSL）
├─ build.ps1                  GCC 构建脚本
├─ flash.bat                  ST-LINK_CLI 命令行烧录（不打开 GUI）
├─ core/                      CMSIS、启动/中断、system_stm32f10x.c
│  ├─ board_config.h          板级配置：HSE 频率、MCU 名称
│  ├─ core_cm3.c              GCC 下已修正 strex 内联汇编约束
│  ├─ syscalls.c              newlib 最小系统调用（仅 GCC 使用）
│  └─ system_stm32f10x.c
├─ startup/startup_stm32f103xe.s   GCC 启动文件
├─ linker/
│  ├─ STM32F103RE.ld          F103RET6：512 KB Flash / 64 KB RAM
│  ├─ STM32F103CB.ld          旧 OneNET Mini：保留作参考
│  └─ STM32F100_EMU.ld        QEMU 仿真：8 KB RAM，仅开发验证
├─ fwlib/                     STM32 标准外设库
├─ hardware/                  delay、usart
├─ user/main.c                应用入口
├─ build/                     GCC 目标文件（生成）
├─ output/
│  └─ gcc/                    GCC 产物
```

> 新增源文件时，要同时加入 Makefile 的 `C_SRCS` 和 `build.ps1` 的 `$Sources`（原生 Windows 路径）。

---

## 6. 和原例程的差异

1. `user/main.c` 改成最小启动验证：打印板卡信息、编译器日期时间、每秒计数。
2. `hardware/src/usart.c` 中 USART1 的 RXNE 中断改为 **DISABLE**。
   原例程开了接收中断，但 `stm32f10x_it.c` 里的 `USART1_IRQHandler` 是注释掉的，收到数据会进入默认死循环。当前只做发送，所以先关闭。
3. 删除 `output/` 里的旧编译产物，避免误烧旧 hex。
4. 新增 GCC 构建支持：
   - `startup/startup_stm32f103xe.s`、`linker/STM32F103RE.ld`
   - `Makefile`、`build.ps1`、`build.bat`
   - `core/syscalls.c`（newlib 的 `_write/_sbrk` 等）
   - 修正 `core/core_cm3.c` 中 GCC 内联汇编的 `=&r` 约束（原写法在 GCC 13 下会汇编失败）
5. 新增 `linker/STM32F100_EMU.ld` 和 `make emu`，仅用于 QEMU 验证。

---

## 7. 后续扩展位置

| 想做的事 | 改哪里 |
| --- | --- |
| 加 LED | 从 `doc/1.../1.LED-流水灯` 复制 `hardware/{inc,src}/led.*`，加入 Makefile 和 build.ps1 |
| 加按键/串口接收 | 在 `usart.c` 打开 RXNE，并在 `stm32f10x_it.c` 实现 `USART1_IRQHandler` |
| 加 SHT20/AT24C02 | 参考 `doc/1.../4.AT24C02`、`5.SHT20-温湿度传感器` |
| 加 ESP8266 + OneNET | 参考 `doc/5.../1.ESP8266-WIFI定位-MQTT`（裸机） |
| 移植 RTOS | 参考 `doc/4...` 的 UCOS/RT-Thread 工程，把 `NET`、`hardware`、`driver` 分层搬过来 |

更完整的资料地图见仓库根目录的 `AGENTS.md`。
