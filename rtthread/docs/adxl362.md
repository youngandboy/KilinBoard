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
| INT1 | PB0，数据就绪中断 |
| INT2 \| PA8，数据就绪中断 |

驱动使用 SPI1 硬件外设，直接操作寄存器，不依赖 RT-Thread SPI
设备框架，避免引入额外的 SPI HAL 代码。

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
4. 配置 `INTMAP1 = DATA_READY`，`INTMAP2 = DATA_READY`。
5. PB0/PA8 配置为下拉输入、上升沿中断。
6. 配置 `FILTER_CTL = 0x03`：±2 g、100 Hz ODR。
7. 配置 `POWER_CTL = 0x02`：测量模式。
8. 等待 20 ms，让第一组数据有效。

启动日志示例：

```text
[adxl] init ok dev=0xad part=0xf2 status=0x40 pwr=0x02 filt=0x03
```

`status=0x40` 表示 AWAKE；数据寄存器约 20 ms 后有效。

---

## 3. 数据格式

普通数据寄存器按 16 位组合：

```c
x_mg = (int16_t)((raw[1] << 8) | raw[0]);
y_mg = (int16_t)((raw[3] << 8) | raw[2]);
z_mg = (int16_t)((raw[5] << 8) | raw[4]);
```

±2 g 量程下 LSB 为 1 mg。

板子水平放置时 Z 轴约 1000 mg；当前实测约 1150–1200 mg，
属于安装/零偏范围，可用 `adxl cal` 校准。

---

## 4. 串口命令

```text
msh > adxl id
adxl: devid=0xad partid=0xf2

msh > adxl read
adxl: X=0 mg Y=-3 mg Z=1001 mg

msh > adxl cal
adxl: calibrated offsets X=51 Y=105 Z=154 mg

msh > adxl reset
adxl: calibration cleared

msh > adxl irq
adxl: INT1 data-ready count=32, INT2 data-ready count=32

msh > adxl fifo
adxl: fifo samples=16 first=48/103/1159 last=49/107/1155 mg
```

`adxl` 不带参数等同于 `adxl read`。

### 零偏校准

`adxl cal` 采样 32 次，按板子水平放置处理：

- X/Y 平均值作为零偏；
- Z 平均值减去 1000 mg 作为零偏，使校准后 Z 约为 +1 g。

偏移只保存在 RAM，复位后清除；需要持久化时再写入 EasyFlash。

### INT1 数据就绪中断

INT1（PB0）和 INT2（PA8）均映射 DATA_READY，使用上升沿 EXTI。`adxl irq` 显示两路累计次数。
常规传感器采集仍按上传周期读取；中断计数用于确认数据就绪和链路健康。

### FIFO 批量读取

`adxl fifo` 使用 STREAM 模式、水位 16，等待约 250 ms 后读取 16 组数据。
ADXL362 FIFO 的每个数据项是 2 字节：B15:B14 为轴类型，B13:B12 为符号扩展，
B11:B0 为 12 位数据；每组样点按 X、Y、Z 排列。驱动按轴类型解析后应用零偏。

读取完成后关闭 FIFO，传感器仍保持测量模式。

---

## 5. 上报数据和倾角

`sensor_task` 每次采样会上报：

```json
{"v":1,"t":"d","seq":1,"up":123,"temp":28.6,"humi":71.4,"light":1593,
 "ax":0,"ay":-3,"az":1001,"valid":7}
```

`valid`：

- bit0：SHT20 有效
- bit1：光照 ADC 有效
- bit2：ADXL362 有效

网页会显示加速度卡片、Pitch/Roll 倾角卡片，并在历史表中增加 `ax/ay/az` 列。
倾角在 PC 端根据三轴 mg 值计算，避免在 MCU 上引入 libm。

---

## 6. 常见问题

| 现象 | 检查 |
| --- | --- |
| `id mismatch dev=0x00 part=0x00` | 检查 PA4–PA7、3.3 V、CS 是否接对 |
| `id mismatch dev=0xff part=0xff` | MISO 未连接或 SPI 模式错误 |
| ID 正常但数据全 0 | 确认 `POWER_CTL` 已进入测量模式；等待 ≥20 ms |
| `adxl irq` 不增长 | 检查 PB0/PA8、INTMAP1/INTMAP2、EXTI0/EXTI8；默认高有效 |
| FIFO 数据异常 | 检查 FIFO 每项 2 字节格式，不要按 6 字节直接分组 |
| 网页没有 ax/ay/az | 确认服务端已重启，浏览器刷新页面 |

---

## 7. 后续可选

- 将零偏写入 EasyFlash，掉电保持。
- 使用 INT1 唤醒采样任务，替代周期轮询。
- 将 INT2 改为 FIFO 水位中断，并配合 DMA 批量读取。
- 增加姿态融合/倾斜角平滑。
