# OTA over Ymodem (kyLin-V3.1)

> 实现位置：`bootloader/`、`app/services/svc_ota.c`、`app/shell/msh_ota.c`、
> `tools/ota_pack.py`、`tools/ota_send.ps1`。

---

## 1. 为什么选 Ymodem，不选 Zmodem

Zmodem 支持窗口、断点续传，但状态机明显更复杂，需要 ZDLE 转义、窗口恢复和更多 RAM/Flash。
当前链路是 PC 的 USB 串口、115200 8N1，传输 220 KB 的本地固件使用 Ymodem-1K 足够可靠。

最终方案：

```text
Ymodem-1K + 每包 CRC16 + 整个镜像 CRC32
```

如果以后需要断点续传，可以在应用层另行增加 Zmodem，不建议放进 16 KB Bootloader。

---

## 2. Flash 分区

| 区域 | 地址 | 大小 | 用途 |
| --- | --- | --- | --- |
| Bootloader | `0x08000000` | 16 KB | Ymodem 接收、校验、复制、跳转 |
| Boot 参数页 | `0x08003800` | 2 KB | 升级状态、版本、大小、CRC |
| App | `0x08004000` | 232 KB | RT-Thread 应用 |
| Download | `0x0803E000` | 232 KB | 新固件临时区 |
| EasyFlash | `0x08078000` | 32 KB | Wi-Fi / 服务器配置，保持不变 |

Bootloader 代码区为 `0x08000000–0x080037FF`，最多 14 KB；参数页占最后一个
2 KB 页。App 链接地址和 `SCB->VTOR` 都必须是 `0x08004000`。

当前 App 约 224 KB，距离 232 KB 上限约 8.3 KB；后续添加功能要持续检查
`arm-none-eabi-size rt-thread.elf`。

---

## 3. 启动流程

```text
上电
  │
  ├─ 读取 Boot 参数页
  ├─ KEY1 按住？──────────────是──> 等待 Ymodem
  ├─ state == DOWNLOAD_VALID？──是──> 校验并复制，然后跳 App
  ├─ state == REQUEST？────────是──> 等待 Ymodem（60 秒无数据则回旧 App）
  ├─ App 向量合法？───────────是──> 跳 App
  └─ 否则 ────────────────────────> 等待 Ymodem / LED 闪烁
```

### 正常 OTA

1. App 中执行 `msh > ota`。
2. App 在参数页写入 `OTA_META_STATE_REQUEST`，然后 `NVIC_SystemReset()`。
3. Bootloader 进入 Ymodem，周期性发送 `C`。
4. PC 发送 `.ota` 容器。
5. Bootloader 写入 Download 区，校验：
   - 容器 magic `KYOTA1`
   - 镜像大小不超过 232 KB
   - 容器头 CRC32
   - 镜像整体 CRC32
6. 校验通过后，复制到 App 区，再次校验 App 区 CRC。
7. 清除升级状态，跳转新 App。

### 失败与恢复

| 情况 | 行为 |
| --- | --- |
| Ymodem 传输失败/CRC 错误 | 参数页回到 IDLE，启动旧 App |
| 传输 60 秒仍未开始 | 参数页回到 IDLE，启动旧 App（如果有） |
| 复制中断/掉电 | 参数页保持 DOWNLOAD_VALID，下次上电自动重新复制 |
| 新 App 无法启动 | 按住 KEY1 复位，强制进入 Bootloader；或 ST-Link 恢复 |
| 没有旧 App | Bootloader 停在等待 Ymodem，LED 闪烁 |

> 512 KB 空间没有 A/B 双备份，因此没有自动回滚到旧 App。Download 区会在
> 复制完成前保留，支持复制阶段的重试；最终恢复手段是 KEY1 + Ymodem 或 ST-Link。

---

## 4. 容器格式

`ota_layout.h` 定义：

```text
offset  size  field
0       8     magic = "KYOTA1\0\0"
8       4     version
12      4     image_size
16      4     image_crc32
20      4     header_crc32（对前 20 字节计算）
24      8     reserved
32      N     rtthread.bin
```

`tools/ota_pack.py` 使用 Python 标准库 `zlib.crc32` 生成容器，不依赖 pyserial。

```powershell
python .\tools\ota_pack.py .\bsp\stm32\stm32f103-kylin-v31\rtthread.bin `
    -o .\rtthread.ota --version 0x1234
```

---

## 5. PC 发送

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ota_send.ps1 `
    -Port COM3 -Image .\rtthread.ota
```

脚本会：

1. 打开串口，发送 `ota\r\n` 让 App 复位到 Bootloader。
2. 等待 Bootloader 的 Ymodem `C`。
3. 发送 1 KB 数据包，逐包等待 ACK/NAK。
4. 发送 EOT 和最终空包头。
5. 等待 Bootloader 完成复制并打印应用启动日志。

如果板子已经在 Bootloader 等待，可以加 `-NoTrigger`，直接发送。

手动使用 Tera Term / Xshell 也可以选择 Ymodem 发送 `.ota` 文件；串口固定为
115200 8N1，不需要流控。

---

## 6. 板端命令

```text
msh > ota status
ota: state=0 version=0x00001234 image=223136 crc=0x3eef9e55

msh > ota
ota: rebooting to bootloader
ota: send rtthread.ota with Ymodem at 115200 8N1
```

`state`：

- `0`：空闲
- `1`：已请求升级
- `2`：Download 有效，等待/正在复制

---

## 7. 构建、烧录和自动验证

首次迁移或恢复时，必须同时烧录 Bootloader 和 App：

```powershell
cd H:\RTOS\OneNet_Mini\firmware
.\flash.bat "rtthread\bootloader\build\bootloader.hex"
.\flash.bat "rtthread\bsp\stm32\stm32f103-kylin-v31\rt-thread.hex"
```

日常验证：

```powershell
cd H:\RTOS\OneNet_Mini\firmware\rtthread
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify-rtthread.ps1 -Port COM3
```

OTA 端到端验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify-ota.ps1 `
    -Port COM3 -Version 0x4321
```

`verify-ota.ps1` 会构建、烧录 Bootloader+App、打包、触发 Ymodem、检查
`[boot] update OK`、`ota status` 版本和 OTA 后的实时 API 数据。证据写入
`rtthread/logs/rtthread-ota-<timestamp>/`。

---

## 8. 已知限制

- 只支持本地串口 Ymodem，不支持网络 OTA。
- 没有固件签名；CRC32 只能防损坏，不能防恶意固件。
- 没有 A/B 自动回滚；依赖 Download 重试、KEY1 和 ST-Link。
- App 槽 232 KB，新增功能要控制体积。
- Bootloader 只在上电/复位后运行，不能把运行中的 App 热替换。
