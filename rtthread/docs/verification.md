# RT-Thread 构建、烧录与板级验证

> 适用目录：`firmware/rtthread/`。本脚本只验证 RT-Thread BSP，不替代
> `firmware/tools/verify-board.ps1`（裸机工程门禁）。

---

## 1. 一键验证

在 `firmware/rtthread/` 目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-rtthread.ps1 -Port COM3
```

成功时最后输出：

```text
RESULT: PASS
```

失败时返回非 0，并输出：

```text
RESULT: FAIL
```

不要把“编译通过”当成板级验证；脚本默认会继续做烧录、串口启动断言和
HTTP API 数据检查。

---

## 2. 前置条件

- WSL 中可用 `arm-none-eabi-gcc`、`scons`
- RT-Thread 源码：默认 `/mnt/h/RTOS/rt-thread-5.3.0`
- `firmware/flash.bat` 能找到 `ST-LINK_CLI.exe`
- 调试串口可用（默认 `COM3`，115200 8N1）
- 验证 API 时，PC 上只运行一个 `server/server.py`
- 板端 EasyFlash 已配置 Wi-Fi 和服务器地址/端口

先确认端口：

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```

先启动 PC 服务：

```powershell
python H:\RTOS\OneNet_Mini\firmware\rtthread\server\server.py
```

---

## 3. 脚本检查内容

| 检查 | 通过条件 |
| --- | --- |
| `BUILD_OK` | `build.sh` 返回 0，生成 App `rt-thread.elf/.hex` 和 Bootloader `.hex` |
| `SIZE_OK` | App ROM ≤ 232 KB，RAM（data + bss）≤ 64 KB |
| `FLASH_OK` | Bootloader 和 App 均烧录成功，输出包含 `Flash OK` |
| `RESET_OK` | ST-LINK CLI 复位成功 |
| `SERIAL_OK` | 抓到 RT-Thread banner、`[app] started`、至少一条 `[sensor]` |
| `API_OK` | `/api/data` 返回 `connected=true`，且有 30 秒内的 `last` 数据 |

串口断言使用 `Thread Operating System`，因为 RT-Thread banner 中
`RT -     Thread` 之间带空格。

---

## 4. 常用参数

```powershell
# 指定串口
... -File tools\verify-rtthread.ps1 -Port COM5

# 只构建，不碰板子
... -File tools\verify-rtthread.ps1 -SkipFlash -SkipSerial -SkipApi

# 板子已经烧好，只抓串口和 API
... -File tools\verify-rtthread.ps1 -SkipBuild -SkipFlash

# 服务器没启动时，跳过 API 检查
... -File tools\verify-rtthread.ps1 -SkipApi

# 不自动复位，直接抓当前串口输出
... -File tools\verify-rtthread.ps1 -SkipBuild -SkipFlash -NoReset
```

默认参数：

| 参数 | 默认值 |
| --- | --- |
| `-Port` | `COM3` |
| `-SerialTimeoutSec` | `25` |
| `-ApiTimeoutSec` | `30` |
| `-ApiUrl` | `http://127.0.0.1:8080/api/data` |
| `-RttRoot` | `/mnt/h/RTOS/rt-thread-5.3.0` |
| `-Scons` | `/home/yu/.local/bin/scons` |

---

## 5. 证据目录

每次运行写入：

```text
rtthread/logs/rtthread-verify-<yyyyMMdd-HHmmss>/
  build.log
  size.txt
  flash-boot.log
  flash.log
  reset.log
  serial.log
  api.json
  summary.md
```

`logs/` 已被 `.gitignore` 忽略，不提交。`summary.md` 包含所有检查项和结果。
例如：

```text
| BUILD_OK | OK | bootloader + app |
| SIZE_OK | OK | ROM=223232 B, RAM=13116 B |
| FLASH_OK | OK | bootloader + app |
| SERIAL_OK | OK | banner=True app=True sensor=True |
| API_OK | OK | seq=0 age=0.2s |
```

---

## 6. 常见失败

| 现象 | 处理 |
| --- | --- |
| `SIZE_OK: FAIL` | 检查是否真的生成了 `rt-thread.elf`；确认 WSL 中有 `arm-none-eabi-size` |
| `FLASH_OK: FAIL` + `Already Used` | 关闭 STM32 ST-LINK Utility GUI |
| `SERIAL_OK: FAIL port ... not found` | 用 `GetPortNames()` 确认 COM 号，或关闭占用串口的串口助手 |
| `SERIAL_OK: FAIL banner=False` | 检查 115200 8N1、PA9/PA10、复位是否成功 |
| `SERIAL_OK: FAIL sensor=False` | 检查 SHT20/ADC 和 `[sensor]` 日志；增加 `-SerialTimeoutSec` |
| `API_OK: FAIL` | 确认 `server.py` 只有一个实例、端口 8080/9000 监听、Windows 防火墙允许 Python |
| `API_OK` 数据过期 | 板端可能未连上服务器，先在串口执行 `net stat` |

板端网络状态：

```text
msh > net stat
```

重点看 `socket=connected`、`tx.frames` 是否增加、`last_error` 和 `dropped`。

---

## 7. 边界

- 脚本不会修改 Wi-Fi 密码、服务器地址或 EasyFlash 配置。
- 脚本不会执行全片擦除（不传 `-Full`）。
- 24 小时稳定性测试是独立项，见 `server/soak.ps1`；本脚本只做一次
  构建 / 烧录 / 启动 / 数据闭环验证。
- OTA 功能使用独立的端到端门禁：
  `tools\verify-ota.ps1`，说明见 `docs/ota.md`。
- ADXL362 可用 `msh > adxl id` / `adxl read` 手动检查，说明见
  `docs/adxl362.md`。
- 如果只想检查裸机工程，继续使用 `firmware/tools/verify-board.ps1`，不要
  删除或改写那套门禁。
