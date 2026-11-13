# AGENTS.md — firmware/（STM32F103 OneNET Mini）

本文件只覆盖 `firmware/`。仓库根目录的 `../AGENTS.md` 是资料地图，**只读参考，不要修改**。

---

## 0. 完成定义（先看这一条）

一个固件任务只有在下面这条命令返回 `RESULT: PASS` 时才算完成：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-board.ps1
```

- **“编译通过”不算完成**，必须有过板证据：烧录 Verify OK + 串口 banner/计数。
- **不允许**为了让门禁通过去修改 `tools/` 里的期望字符串、阈值或脚本。
- 环境问题（COM 被占用、ST-LINK GUI 占用、探针没插）**报告给人**，不要绕过门禁或伪造日志。
- 没有板子时，可以只跑 `tools\verify-build.ps1`，并明确说明“未上板验证”。

---

## 1. 硬件事实（不要猜）

| 项目 | 值 |
| --- | --- |
| MCU | STM32F103RET6，Cortex-M3，72 MHz，LQFP64，512 KB Flash / 64 KB RAM |
| 晶振 | 12 MHz（PLL ×6，已实测串口 115200 正常）；若换 8 MHz 板改 `core/board_config.h` 的 `BOARD_HSE_HZ` |
| 调试串口 | USART1，PA9(TX)/PA10(RX)，115200 8N1；端口用 `tools\serial-capture.ps1 -ListPorts` 确认（CH340 或 ST-LINK VCP） |
| 模块串口 | USART2，PA2(TX)/PA3(RX)，接 ESP8266 |
| ESP8266 复位 | PB5，代码里叫 `nd_rst`，**不要改名** |
| LED | PB6/PB7/PB8/PB9，**低电平点亮** |
| I2C | I2C2，PB10(SCL)/PB11(SDA)；SHT20 `0x40`，AT24C02 `0x50` |
| 烧录 | ST-Link，SWD，不需要接 reset |
| Flash/RAM | 512 KB / 64 KB；GCC 链接脚本为 `linker/STM32F103RE.ld` |

引脚/外设改动前，先读 `README.md` 和根目录 `AGENTS.md` 的硬件章节。

---

## 2. 构建、烧录、验证命令

在 `firmware/` 目录下执行：

```powershell
# 完整门禁：构建 -> 烧录 -> 串口断言 -> 尺寸预算 -> 证据
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-board.ps1

# 只构建（ARM GCC）
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-build.ps1

# 只构建，等价于 build.bat / make
build.bat
make

# 只烧录
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-flash.ps1

# 只抓串口（先看端口）
powershell -NoProfile -ExecutionPolicy Bypass -File tools\serial-capture.ps1 -ListPorts
powershell -NoProfile -ExecutionPolicy Bypass -File tools\serial-capture.ps1 -Port COMx -Reset
```

注意区分：

- `build.bat` / `build.ps1` / `Makefile` = ARM GCC 构建（Windows 没有工具链时自动走 WSL）
- `tools\verify-build.ps1` = 门禁包装器，**不是**替代品；它会写 `logs/build.json`
- `flash.bat` = 实际烧录；`tools\verify-flash.ps1` 只是包装并解析结果

产物：

- ARM GCC：`output/gcc/OneNet_Mini_Bare.hex`

---

## 3. 证据与失败处理

`verify-board.ps1` 成功后会把证据写到：

```
docs/EVIDENCE/<yyyyMMdd-HHmmss>/
  summary.md
  build.json / flash.json / serial.json
  build-gcc.log / flash.log / serial.log
  size.txt
  size-baseline.json
```

失败时按阶段处理：

| 阶段 | 先看 | 常见原因 |
| --- | --- | --- |
| build | `logs/build-gcc.log` | 语法/链接错误；新文件没加进 Makefile / build.ps1 |
| flash | `logs/flash.log` | ST-LINK GUI 占用探针；HEX 路径不对；供电/接线 |
| serial | `logs/serial.log` | COM 被串口助手占用；波特率不对；没复位导致漏掉 banner |
| size | `logs/size.txt` 与 `tools/size-baseline.json` | Flash/RAM 增长超过 5%；释放缓冲区、改优化等级 |

失败时**先修根因，再重跑**；连续两次同类失败，把规则补进本文件或 `tools/`（harness engineering）。

---

## 4. 目录与改动边界

| 目录 | 职责 | 备注 |
| --- | --- | --- |
| `user/` | 应用入口、主循环、后续任务编排 | 不放寄存器细节 |
| `hardware/` | 板级设备：led、sht20、at24c02、info | 通过 driver 访问外设 |
| `driver/` | 片内外设：usart、i2c、delay、iwdg、rtc、hwtimer | 可复用，不掺业务 |
| `net/` | 网络设备层与协议（后续加入） | 不要把 ESP8266 AT 逻辑塞进 `main.c` |
| `core/`、`fwlib/` | 启动、CMSIS、标准外设库 | 除已知 GCC 修正外不要改 |
| `doc/` | 原始例程与资料 | **只读，不要修改** |
| `tools/` | 构建/烧录/串口/门禁脚本 | 改脚本视为改门禁，需人审 |
| `skills/` | agent 工作流说明 | 与实际脚本保持同步 |

新增源文件必须同时更新：

1. `Makefile` 的 `C_SRCS`
2. `build.ps1` 的 `$Sources`

---

## 5. 硬性规则

- **一次只改一个功能点**；不要顺手重构无关文件。
- 不修改 `doc/` 下的原始例程，不把多个例程的 `NET/` 混拷。
- 不把 WiFi 密码、APIKEY、设备 ID 等真实密钥写进仓库。
- ISR 里不做阻塞操作、不 `printf`、不 `malloc`；快进快出，耗时逻辑丢到主循环/任务。
- 共享变量用 `volatile`；跨任务/中断通信用明确的同步原语。
- 所有返回错误码的函数都要检查并处理，不要静默忽略。
- 改动 ISR、DMA、时钟、链接脚本、启动文件、中断向量时，**必须在计划里标红并请人审**。
- 提交前跑门禁，把 `docs/EVIDENCE/.../summary.md` 路径写进汇报。

---

## 6. 已知坑

1. **编码混合**：`user/`、`hardware/`、`driver/`、`net/` 和 `core/system_stm32f10x.c` 是 GBK；`fwlib/`、`core/stm32f10x.h` 等多数是 UTF-8/ASCII。不要全仓转 UTF-8，否则中文注释乱码、diff 爆炸。
2. **ST-LINK Utility GUI 会独占探针**：CLI 报 `ST-LINK Already Used`，先关 GUI。
3. **调试串口可能被串口助手占用**：旧板是 COM3，新板可能是 ST-LINK VCP；先关占用程序，用 `serial-capture.ps1 -ListPorts` 确认端口。
4. **漏抓 banner**：烧录后程序立刻运行，直接监听会错过启动行。用 `-Reset` 让脚本先开串口再复位。
5. **晶振已确认**：`core/board_config.h` 当前 `BOARD_HSE_HZ = 12000000`（12 MHz，PLL ×6）。如果换成 8 MHz 板，改成 `8000000`；否则 PLL 会跑偏，串口乱码。
6. **USART1 接收中断是关的**：`usart.c` 里 `USART_IT_RXNE` 为 DISABLE，且没有 `USART1_IRQHandler`；要收数据先补中断处理。
7. **`sample.c` 不参与编译**：改它不会生效。
8. **浮点 printf 的任务栈要 8 字节对齐**（引入 RTOS 后）。
9. **`-Full` 会全片擦除**：除非明确要求，不要用。

---

## 7. 推荐工作流

1. 读本文件 + `README.md` + 相关源码，确认硬件事实。
2. 写一个短计划：改哪些文件、为什么、怎么验证（对应 `verify-board` 的哪条断言）。
3. 实施，尽量小步。
4. 跑 `tools\verify-board.ps1`。
5. 失败：读日志定位阶段，修根因，重跑；不要改门禁。
6. 通过：汇报 `RESULT: PASS` + 证据目录 + 改动摘要 + 未覆盖的风险。
7. 如果这次犯了新错误：补一条规则到本文件，或加一个检查到 `tools/`。

汇报模板：

```text
做了什么：
改了哪些文件：
验证：RESULT: PASS（证据 docs/EVIDENCE/<ts>/）
未验证/风险：
```

---

## 8. 相关文件

- `README.md`：构建、烧录、串口、已验证程度
- `../AGENTS.md`：仓库资料地图（只读）
- `skills/verify-board/SKILL.md`：门禁的使用说明与失败手册
- `docs/EVIDENCE/README.md`：证据目录约定
- `tools/size-baseline.json`：尺寸基线（首次运行自动生成，改动需人审）
