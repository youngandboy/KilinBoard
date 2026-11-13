# EVIDENCE — 每次上板验证的证据

由 `tools/verify-board.ps1` 自动生成，每次运行一个时间戳目录：

```
docs/EVIDENCE/<yyyyMMdd-HHmmss>/
  summary.md              本次门禁结论（自动生成，不要手改）
  build.json              构建结果、错误列表、尺寸
  flash.json              烧录结果与信号（Verify OK / Application started）
  serial.json             串口断言结果与缺失项
  build-keil.log          原始构建日志（或 build-gcc.log）
  flash.log               ST-LINK_CLI 原始输出
  serial.log              串口原始输出
  size.txt                本次 Flash/RAM
  size-baseline.json      当时的尺寸基线
```

约定：

- 一个 PR / 一次任务至少保留一个 PASS 的证据目录。
- 失败目录也保留，方便复盘；不要用新结果覆盖旧的。
- `summary.md` 是机器生成的审计记录，不要手工编辑结论。
- 证据里不要出现真实密钥、WiFi 密码、设备 ID。
- 仓库只提交小的文本证据；大体积波形/截图放外部存储，在 `summary.md` 里写链接。
- `legacy_f103c8/` 是旧 OneNET Mini（F103C8，64 KB/20 KB）的历史证据，只作参考，不代表当前 F103RET6 板。
