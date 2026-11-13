# OneNet_Mini_Bare —— STM32F103 裸机起步工程

基于 `doc/1.OneNET-Mini-V1.x-基础学习例程/2.串口打印` 复制出来的独立工程，用来验证：

**上电 → 启动文件 → `main()` → USART1 打印 → 死循环**

当前不包含 RTOS，后续可以在这套工程上继续加外设、再加 UCOS/RT-Thread。
同时提供 **Keil 工程** 和 **命令行脚本（GCC）** 两种编译方式。

---

## 1. 工程信息

| 项目 | 内容 |
| --- | --- |
| Keil 工程 | `OneNet_Mini_Bare.uvprojx` |
| GCC 脚本 | `build.bat` / `build.ps1` / `Makefile` |
| Keil 脚本 | `build_keil.bat` |
| 目标器件 | STM32F103CB（实际板载 CBT6，128 KB Flash / 20 KB SRAM） |
| 系统时钟 | 72 MHz（外部 12 MHz 晶振，PLL ×6） |
| 调试串口 | USART1，PA9(TX) / PA10(RX)，115200 8N1 |
| 下载方式 | ST-Link，SWD，Keil 中选择 **SW** 模式 |
| 入口文件 | `user/main.c` |
| 延时 | `hardware/src/delay.c`，SysTick 阻塞延时 |
| 串口打印 | `hardware/src/usart.c` 的 `UsartPrintf()` |

产物位置：

- Keil：`output/OneNet_Mini_Bare.hex`
- GCC 脚本：`output/gcc/OneNet_Mini_Bare.hex`

---

## 2. 编译方式

### 2.1 脚本编译（推荐，不依赖 Keil）

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

### 2.2 Keil 编译

1. 用 Keil MDK5 打开 `firmware/OneNet_Mini_Bare.uvprojx`。
2. 确认 Target 为 `OneNet_Mini_Bare`，器件为 `STM32F103CB`。
3. `Options for Target → Debug` 选择 ST-Link，Port 选 **SW**。
4. F7 编译，F8 下载。
5. 生成 `output/OneNet_Mini_Bare.hex`。

> Keil 和 GCC 产物分开放：Keil 用 `output/`，GCC 用 `output/gcc/`，避免互相覆盖。

### 2.3 Keil 命令行编译

如果不想打开 Keil 界面，可以直接运行：

```bat
build_keil.bat
build_keil.bat -Rebuild
```

它会自动查找 `UV4.exe`，调用 `UV4 -b` / `-r` 编译，并输出 `output/OneNet_Mini_Bare.hex`。

如果要用 Keil GUI，建议从 `keil_gui.bat` 启动工程，它会先在当前进程清空 ADS 环境变量，再打开 Keil。

> **重要环境坑**：这台机器上装过旧版 ARM ADS 1.2，系统环境变量 `ARMCONF`、`ARMDLL`、`ARMHOME`、`ARMINC`、`ARMLIB` 会让 Keil ARMCC5 错误地使用
> `C:\Program Files (x86)\ARM\ADSv1_2\INCLUDE\stdarg.h`，表现为：
>
> ```
> usart.c(190): error: #928: incorrect use of va_start
> ```
>
> `build_keil.bat` 已经在调用 UV4 前清空这些变量。如果直接用 Keil GUI 编译也遇到这个错误，
> 从“系统环境变量”里删除这 5 个变量（或至少删除 `ARMINC`）后重启 Keil。
> GCC 构建不受影响。

### 2.4 ST-LINK 命令行烧录

脚本：`flash.bat`。默认烧 `output/OneNet_Mini_Bare.hex`（Keil 产物）；
如果它不存在，自动改用 `output/gcc/OneNet_Mini_Bare.hex`。

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
 MCU  : STM32F103CBT6 @ 72MHz
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

- GCC 构建：在 WSL Ubuntu 24.04 下用 `arm-none-eabi-gcc 13.2.1` 编译链接通过，无警告。
- Keil 构建：用 `build_keil.bat -Rebuild` 实测通过，0 Error / 0 Warning，
  `Code=10728, RO-data=784, RW-data=44, ZI-data=2148`。
- 运行验证：用 QEMU `stm32vldiscovery`（STM32F100，8 KB RAM 的临时链接脚本）运行，
  能正常打印上面的启动信息和递增的 `counter`。
- 真实板子烧录：用 `flash.bat` + `ST-LINK_CLI.exe` 实测通过，
  `Device ID: 0x410`、`Device flash Size: 64 Kbytes`、目标电压 3.2 V，
  `Verification...OK`、`Application started`。
- 串口 `COM3`（CH340）当时被其他程序占用，未抓取真实板子的打印输出；
  关闭占用程序后可在 115200 8N1 下查看。

> **注意**：ST-LINK 读出的 Flash 大小是 **64 KB**，与 BOM/手册里的 `STM32F103CBT6 / 128 KB` 不一致。
> 当前工程按 `STM32F103CB`（128 KB）配置，但生成的代码只有约 10 KB，完全放得下。
> 如果确认板载是 C8（64 KB），建议把 Keil Target 改为 `STM32F103C8`，
> 并把 GCC 链接脚本换成 64 KB 版本，避免以后代码超过 64 KB 时才发现问题。

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
├─ OneNet_Mini_Bare.uvprojx   Keil MDK5 工程（ARMCC）
├─ Makefile                   GCC 构建
├─ build.bat                  GCC 构建入口（Windows / WSL）
├─ build.ps1                  GCC 构建脚本
├─ build_keil.bat             Keil 命令行构建（会清理 ADS 环境变量）
├─ keil_gui.bat               用干净环境打开 Keil 工程
├─ flash.bat                  ST-LINK_CLI 命令行烧录（不打开 GUI）
├─ keilkilll.bat              Keil 中间文件清理
├─ core/                      CMSIS、启动/中断、system_stm32f10x.c
│  ├─ core_cm3.c              GCC 下已修正 strex 内联汇编约束
│  ├─ syscalls.c              newlib 最小系统调用（仅 GCC 使用）
│  ├─ startup/arm/            Keil 启动文件（原样保留）
│  └─ startup/                GCC 启动文件 startup_stm32f103xb.s
├─ linker/
│  ├─ STM32F103CB.ld          真实板子：128 KB Flash / 20 KB RAM
│  └─ STM32F100_EMU.ld        QEMU 仿真：8 KB RAM，仅开发验证
├─ fwlib/                     STM32 标准外设库
├─ hardware/                  delay、usart
├─ user/main.c                应用入口
├─ build/                     GCC 目标文件（生成）
├─ output/                    Keil 产物
│  └─ gcc/                    GCC 产物
├─ listing/                   Keil 列表（生成）
├─ RTE/                       Keil RTE 配置
└─ DebugConfig/               Keil 调试配置
```

> 新增源文件时，要同时加入 Makefile 的 `C_SRCS` 和 `build.ps1` 的 `$Sources`（原生 Windows 路径），
> 以及 Keil 工程的对应分组。

---

## 6. 和原例程的差异

1. `user/main.c` 改成最小启动验证：打印板卡信息、编译器日期时间、每秒计数。
2. `hardware/src/usart.c` 中 USART1 的 RXNE 中断改为 **DISABLE**。
   原例程开了接收中断，但 `stm32f10x_it.c` 里的 `USART1_IRQHandler` 是注释掉的，收到数据会进入默认死循环。当前只做发送，所以先关闭。
3. 删除 `output/`、`listing/` 里的旧编译产物，避免误烧旧 hex。
4. 新增 GCC 构建支持：
   - `startup/startup_stm32f103xb.s`、`linker/STM32F103CB.ld`
   - `Makefile`、`build.ps1`、`build.bat`
   - `core/syscalls.c`（newlib 的 `_write/_sbrk` 等）
   - 修正 `core/core_cm3.c` 中 GCC 内联汇编的 `=&r` 约束（原写法在 GCC 13 下会汇编失败）
5. 新增 `linker/STM32F100_EMU.ld` 和 `make emu`，仅用于 QEMU 验证。
6. 新增 `build_keil.bat`：调用 Keil `UV4.exe` 前清空 ARM ADS 1.2 的 `ARMCONF/ARMDLL/ARMHOME/ARMINC/ARMLIB`，否则 ARMCC5 会误用 ADS 的 `stdarg.h`。

---

## 7. 后续扩展位置

| 想做的事 | 改哪里 |
| --- | --- |
| 加 LED | 从 `doc/1.../1.LED-流水灯` 复制 `hardware/{inc,src}/led.*`，加入 Keil 分组和 Makefile |
| 加按键/串口接收 | 在 `usart.c` 打开 RXNE，并在 `stm32f10x_it.c` 实现 `USART1_IRQHandler` |
| 加 SHT20/AT24C02 | 参考 `doc/1.../4.AT24C02`、`5.SHT20-温湿度传感器` |
| 加 ESP8266 + OneNET | 参考 `doc/5.../1.ESP8266-WIFI定位-MQTT`（裸机） |
| 移植 RTOS | 参考 `doc/4...` 的 UCOS/RT-Thread 工程，把 `NET`、`hardware`、`driver` 分层搬过来 |

更完整的资料地图见仓库根目录的 `AGENTS.md`。
