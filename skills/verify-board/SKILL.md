---
name: verify-board
description: Build, flash, and prove STM32 firmware on the real board. Use before claiming any firmware task is done, after changing drivers/USART/main, or when asked to verify hardware behavior, capture serial output, or check flash/RAM budgets.
---

# Verify board

把“改完代码”变成“有证据的完成”。这个 skill 只做一件事：跑门禁，读结果，按失败阶段修，直到 `RESULT: PASS`。

## 何时使用

- 改完 `user/`、`hardware/`、`driver/`、`net/` 里任何会影响运行的代码
- 准备说“完成了”“修好了”“已经验证过”
- 需要抓串口输出、确认板子真的在跑
- 需要检查 Flash/RAM 是否超预算
- 上板行为与预期不符，需要分层定位

## 命令

在 `firmware/` 目录下：

```powershell
# 完整门禁（ARM GCC）
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-board.ps1

# 指定端口 / 超时
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-board.ps1 -Port COM3 -TimeoutSec 20

# 分步排查
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-flash.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\serial-capture.ps1 -ListPorts
powershell -NoProfile -ExecutionPolicy Bypass -File tools\serial-capture.ps1 -Port COM3 -Reset

# 只看构建（没有板子时）
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-build.ps1
```

## PASS 的含义

必须同时满足：

1. `RESULT: BUILD_OK`：真实工具链接受工程，无新增 error
2. `RESULT: FLASH_OK`：ST-LINK 报告 `Verification...OK` / `Flash OK`
3. `RESULT: SERIAL_OK`：抓到启动 banner、`Hello from main()`，并且 `counter` 至少从 1 涨到 2（证明主循环在推进）
4. `size` 门禁：Flash/RAM 相对基线增长不超过 5%
5. `RESULT: PASS`，并生成 `docs/EVIDENCE/<timestamp>/summary.md`

任何一条不满足，都不算完成。

## 失败处理

| 输出 | 先看 | 处理 |
| --- | --- | --- |
| `BUILD_FAIL` | `logs/build-gcc.log` | 修语法/链接；新文件是否加进 Makefile、build.ps1 |
| `FLASH_FAIL reason=no_hex` | 产物路径 | 先跑 `verify-build.ps1` |
| `FLASH_FAIL` + `Already Used` | ST-LINK GUI | 关掉 STM32 ST-LINK Utility，重跑 |
| `SERIAL_FAIL reason=no_port` | `-ListPorts` | 换 `-Port COMx`；确认 CH340 驱动 |
| `SERIAL_FAIL reason=open_or_read` | 串口占用 | 关串口助手/PuTTY，重跑 |
| `SERIAL_FAIL missing=...` | `logs/serial.log` | 程序没跑/波特率错/漏抓 banner；加 `-Reset` 或按复位键 |
| `size FAIL` | `size.txt` + `size-baseline.json` | 释放缓冲、改优化、确认是否误引入大表 |
| 超时 | 探针/供电 | 检查 SWD 接线、目标供电；不要反复重试刷屏 |

连续两次同类失败：把规则补进 `AGENTS.md`，或把检查加进 `tools/`。**不要**修改期望字符串、阈值或脚本让门禁变绿。

## 硬性约束

- 不要编辑 `tools/` 下的期望字符串、阈值、基线来伪造 PASS。
- 不要默认用 `-Full`（全片擦除）；只有人明确要求时。
- 不要并行烧同一块板；一次一个烧录/串口会话。
- 失败时保留日志，不要删 `docs/EVIDENCE/` 或 `logs/`。
- 没有板子时，明确说“只做了构建验证，未上板”，不要声称 PASS。

## 汇报模板

```text
改动：
- <file>: <what>

验证：RESULT: PASS
证据：docs/EVIDENCE/<timestamp>/
  build: flash=<N> B ram=<N> B growth=<x>%
  flash: Verify OK
  serial: banner + counter
未验证/风险：
- <e.g. 未做长时间稳定性测试>
```
