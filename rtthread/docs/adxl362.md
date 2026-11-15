# ADXL362 三轴加速度计

> 驱动：`app/drivers/drv_adxl362.c`
> 命令：`app/shell/msh_adxl.c`
> 采集：`app/services/svc_sensor.c`、`app/tasks/task_sensor.c`

---

## 1. 硬件连接

kyLin-V3.1 的 U8 为 ADXL362，使用 SPI1：

| 信号 | MCU 引脚 |
| --- | --- |
| CS | PA4 |
| SCK | PA5 |
| MISO | PA6 |
| MOSI | PA7 |
| INT1 | PB0（保留） |
| INT2 | PA8（保留） |

当前驱动使用轮询读取，不启用 INT1/INT2。CS 由软件控制，SPI1 使用硬件外设，
但不依赖 RT-Thread SPI 设备框架，避免引入额外的 SPI HAL 代码。

SPI 参数：

```text
模式 3（CPOL=1, CPHA=1）
8 位
4.5 MHz（72 MHz / 16）
```

---

## 2. 初始化和量程

驱动启动时：

1. 配置 PA4–PA7 和 SPI1。
2. 软复位 ADXL362（写 `0x52` 到 `0x1F`）。
3. 读取 `DEVID_AD`（期望 `0xAD`）和 `PARTID`（期望 `0xF2`）。
4. 配置 `FILTER_CTL = 0x03`：
   - 量程 ±2 g
   - ODR 100 Hz
5. 配置 `POWER_CTL = 0x02`：测量模式。
6. 读取一次状态、电源和滤波寄存器用于确认初始化。

启动日志示例：

```text
[adxl] init ok dev=0xad part=0xf2 status=0x40 pwr=0x02 filt=0x03
```

`status=0x40` 表示 AWAKE；数据寄存器约 20 ms 后有效。

---

## 3. 数据格式

ADXL362 的 X/Y/Z 数据寄存器为 12 位有符号值，按 16 位组合：

```c
x_mg = (int16_t)((raw[1] << 8) | raw[0]);
y_mg = (int16_t)((raw[3] << 8) | raw[2]);
z_mg = (int16_t)((raw[5] << 8) | raw[4]);
```

±2 g 量程下 LSB 为 1 mg，因此原始值可直接按 mg 使用。

板子水平放置时 Z 轴约为 1000 mg；实际板子可能有安装偏置，当前实测约
`1160 mg`，属于机械安装/零偏范围，需要的话可在应用层做零偏校准。

---

## 4. 串口命令

```text
msh > adxl id
adxl: devid=0xad partid=0xf2

msh > adxl read
adxl: X=24 mg Y=-9 mg Z=1163 mg
```

`adxl` 不带参数等同于 `adxl read`。

---

## 5. 上报数据

`sensor_task` 每次采样会上报：

```json
{"v":1,"t":"d","seq":1,"up":123,"temp":28.6,"humi":71.4,"light":1593,
 "ax":25,"ay":-16,"az":1163}
```

网页会显示加速度卡片，并在历史表中增加 `ax/ay/az` 列。

ADXL362 读取失败时不会阻止 SHT20/光照上报；`valid` 的 bit2 用于标记
ADXL362 是否有效。

---

## 6. 常见问题

| 现象 | 检查 |
| --- | --- |
| `id mismatch dev=0x00 part=0x00` | 检查 PA4–PA7、3.3 V、CS 是否接对 |
| `id mismatch dev=0xff part=0xff` | MISO 未连接或 SPI 模式错误 |
| ID 正常但数据全 0 | 确认 `POWER_CTL` 已进入测量模式；等待 ≥20 ms |
| 数据抖动大 | 检查电源、SPI 线长度；可降低 SPI 速率 |
| 网页没有 ax/ay/az | 确认服务端已重启，浏览器刷新页面 |

---

## 7. 后续可选

- 使用 INT1（PB0）数据就绪中断，替代轮询。
- 增加零偏校准和倾斜角计算。
- 使用 FIFO 批量读取，降低采样任务唤醒次数。
