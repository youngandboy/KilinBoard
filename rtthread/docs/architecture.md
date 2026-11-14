# RT-Thread 应用架构设计（kyLin-V3.1）

> 适用范围：`firmware/rtthread/` 下的应用层，Step 2 起生效。
> 约束：STM32F103RET6，64 KB RAM；无 MQTT/SNTP/TLS/lwIP；ESP8266 AT Socket；调试主要靠 USART1 msh/ulog。

---

## 1. 设计目标

1. **结构化**：板级、驱动、服务、任务、协议、shell 分层，避免逻辑堆在 `main.c`。
2. **可扩展**：Step 2 的 LED/KEY/存储、Step 3 的传感器、Step 4 的网络、Step 5 的协议都能按同一套接口加进来。
3. **省 RAM**：任务栈、消息队列、缓冲区全部静态分配；运行期不依赖 `rt_malloc`。
4. **可调试**：每个模块有 msh 命令；关键状态、错误码、任务栈水位可查。
5. **可测试**：模块 API 只依赖 RT-Thread 设备/IPC，不直接操作寄存器；能通过 msh 单独验证。
6. **可恢复**：网络、传感器、存储故障不能拖死整个系统；看门狗只由健康检查任务喂。

---

## 2. 三个候选方案

### 方案 A：分层服务 + 少量任务 + 静态消息队列（推荐）

```text
msh / ulog
   │
tasks/        sensor_task   net_task   supervisor_task
   │              │            │             │
services/     svc_sensor   svc_net      svc_watchdog
   │              │            │
drivers/      drv_sht20    svc_protocol  svc_storage
   │           drv_at24c02
board/        board_led / board_key / board_buzzer
   │
RT-Thread device / pin / wdt / fal / easyflash / SAL
```

- 3 个任务：sensor、net、supervisor；
- 任务间用 `rt_mq` 传消息，用 `rt_event` 传状态位；
- 初始化阶段直接调用服务 API，运行期不互相直接调用阻塞函数；
- 每个模块一对 `.c/.h`，API 用 `int`/`rt_err_t` 返回错误码。

**优点：**

- 边界清楚，Step 3/4/5 只需加模块，不改主流程；
- 网络阻塞不会卡住传感器采样；
- 每个模块可单独用 msh 验证；
- 静态分配可控，64 KB RAM 足够；
- 适合后续加协议、OTA、状态机。

**缺点：**

- 文件/接口比单文件多；
- 需要设计消息结构和任务优先级；
- 任务栈/队列大小要实测。

### 方案 B：单线程事件驱动 + 状态机

- 只有 1 个 app 线程，循环里 `select`/`poll` + 定时器；
- 所有模块是状态机，不互相阻塞。

**优点：**

- RAM 最省，任务切换最少；
- 没有并发/锁问题，逻辑确定。

**缺点：**

- ESP8266 AT 的 socket 阻塞/超时容易拖住传感器采样；
- 一个模块卡住会拖死整个循环；
- 所有状态机揉在一起，Step 4/5 后难以维护；
- msh 命令与主循环共享上下文，调试不方便。

### 方案 C：消息总线 / Actor 模型

- 每个模块一个 actor + 邮箱，中央 bus 转发；
- 模块之间完全解耦。

**优点：**

- 扩展性最好，新增模块不改别人；
- 适合大系统/多协议。

**缺点：**

- 对 3~5 个模块来说过度设计；
- 每个 actor 都要栈/队列，64 KB RAM 压力大；
- 调试链路长，问题定位困难；
- 当前项目不需要这种复杂度。

### 结论

**选方案 A。** 保留“分层 + 少量任务 + 静态消息队列”，不引入消息总线和额外 actor；网络和传感器各一个任务，supervisor 单独喂狗。

---

## 3. 目录结构

```text
firmware/rtthread/bsp/stm32/stm32f103-kylin-v31/
├─ applications/
│  └─ main.c                 # 只调用 app_main()
├─ app/
│  ├─ app_main.c/.h          # 应用入口：初始化、建队列、建任务
│  ├─ app_config.h           # 引脚、周期、栈大小、优先级、缓冲区大小
│  ├─ app_types.h            # 错误码、消息结构、状态枚举
│  ├─ app_ipc.c/.h           # 静态消息队列 + 事件集封装
│  ├─ SConscript
│  ├─ board/
│  │  ├─ board_led.c/.h      # 4 LED，低电平点亮
│  │  ├─ board_key.c/.h      # 4 KEY，扫描+消抖
│  │  └─ board_buzzer.c/.h   # PB3 蜂鸣器
│  ├─ drivers/
│  │  ├─ drv_at24c02.c/.h    # Step 3
│  │  ├─ drv_sht20.c/.h      # Step 3
│  │  ├─ drv_light.c/.h      # Step 3
│  │  └─ drv_adxl362.c/.h    # Step 6 可选
│  ├─ services/
│  │  ├─ svc_storage.c/.h    # FAL + EasyFlash 参数
│  │  ├─ svc_sensor.c/.h     # Step 3：采样聚合
│  │  ├─ svc_net.c/.h        # Step 4：AT Socket 状态机
│  │  ├─ svc_protocol.c/.h   # Step 5：帧编解码
│  │  └─ svc_watchdog.c/.h   # 健康位 + IWDG
│  ├─ tasks/
│  │  ├─ task_sensor.c/.h    # 按键扫描、传感器采样
│  │  ├─ task_net.c/.h       # Step 4：网络收发
│  │  └─ task_supervisor.c/.h# 1 s 健康检查 + 喂狗
│  ├─ shell/
│  │  ├─ msh_board.c         # led/key/beep
│  │  ├─ msh_sensor.c        # Step 3
│  │  ├─ msh_net.c           # Step 4
│  │  └─ msh_cfg.c           # cfg get/set/save/reset
│  └─ port/
│     ├─ fal_cfg.h           # FAL 分区表
│     └─ ef_fal_port.c       # EasyFlash 的 FAL 移植
└─ ...
```

原则：

- `board/` 只做引脚/板级动作，不做业务；
- `drivers/` 只做器件时序/寄存器，不依赖任务；
- `services/` 组合驱动，提供业务能力；
- `tasks/` 只做调度和状态机，不直接碰寄存器；
- `shell/` 只调用 service/board API；
- `app_main.c` 只做初始化和启动，不写业务循环。

---

## 4. 任务与 IPC

| 任务 | 优先级 | 栈 | 周期/触发 | 职责 |
| --- | ---: | ---: | --- | --- |
| `supervisor` | 11 | 768 B | 1 s | 检查健康位、喂 IWDG、记录栈水位 |
| `sensor` | 12 | 1024 B | 10 ms 键扫描 / 1 s 采样 | 读 KEY、SHT20、光敏，发消息 |
| `net` | 14 | 2048 B | socket 事件/1 s | ESP8266、TCP 收发、重连 |
| `tshell` | 20 | 4096 B | msh | 调试命令 |
| `main` | 10 | 2048 B | 一次 | 调 `app_main()` 后挂起 |

IPC：

- `app_mq`：静态消息队列，深度 16，传 `app_msg_t`；
- `app_events`：事件集，位定义：
  - `APP_EVT_SENSOR_ALIVE`
  - `APP_EVT_NET_ALIVE`
  - `APP_EVT_NET_CONNECTED`
  - `APP_EVT_CFG_CHANGED`
- `svc_storage` 内部用 mutex 保护配置；
- 运行期不使用动态内存；所有队列池、缓冲区都是 `static`。

消息结构（Step 2 先定义，Step 3/4 扩展）：

```c
typedef enum {
    APP_MSG_NONE = 0,
    APP_MSG_KEY,
    APP_MSG_SENSOR,
    APP_MSG_NET_STATUS,
    APP_MSG_CMD,
} app_msg_type_t;

typedef struct {
    app_msg_type_t type;
    union {
        struct { uint8_t id; uint8_t pressed; } key;
        struct { int16_t temp_x10; uint16_t humi_x10; uint16_t light; } sensor;
        struct { uint8_t connected; } net;
        struct { uint8_t cmd; uint8_t arg; } cmd;
    } u;
} app_msg_t;
```

---

## 5. 数据流与状态机

### 5.1 传感器 → 网络

```text
sensor_task
  ├─ 每 10 ms：board_key_scan()
  ├─ 每 1 s：SHT20 / 光敏采样
  └─ app_mq_send(APP_MSG_SENSOR / APP_MSG_KEY)

net_task
  ├─ 等待 APP_MSG_SENSOR
  ├─ svc_protocol_encode()
  └─ socket send
```

Step 2 还没有 SHT20/网络，`sensor_task` 先做：

- 10 ms 扫描 4 个按键；
- 按键变化时打印并翻转对应 LED（验证 GPIO）；
- 每 1 s 发一条 `APP_MSG_SENSOR` 空样本/心跳到队列（验证 IPC）；
- 设置 `APP_EVT_SENSOR_ALIVE`。

### 5.2 网络状态机（Step 4）

```text
IDLE
  → WIFI_CONNECTING
  → WIFI_READY
  → TCP_CONNECTING
  → ONLINE
  → RECONNECT_WAIT → WIFI_CONNECTING
```

- 失败原因分级：模组 / Wi-Fi / DHCP / DNS / TCP / 对端关闭；
- 指数退避：1 s、2 s、4 s、8 s，上限 30 s；
- `net_task` 独占 socket，其他任务不能直接调 socket；
- 连接状态通过 `APP_EVT_NET_CONNECTED` 广播。

### 5.3 看门狗

- `supervisor_task` 每 1 s 检查 `APP_EVT_SENSOR_ALIVE` 和（Step 4 后）`APP_EVT_NET_ALIVE`；
- 所有必需位在本周期内都置位才喂狗；
- 某任务卡死 → 不喂狗 → IWDG 复位；
- 不允许在中断、socket 发送循环里直接喂狗。

---

## 6. 编码规则

1. 命名：
   - 板级：`board_led_*`、`board_key_*`；
   - 驱动：`drv_sht20_*`；
   - 服务：`svc_storage_*`；
   - 任务：`task_sensor_*`；
   - 应用：`app_*`。
2. 返回值：统一 `app_err_t` 或 `rt_err_t`；禁止静默忽略错误。
3. 日志：`LOG_TAG` + `LOG_I/W/E`；Step 2 启用 ulog。
4. 断言：只用于“不可能发生”的编程错误；运行期错误用返回码。
5. ISR：不做阻塞、不 `printf`、不 `malloc`；只置事件/发信号。
6. 内存：静态分配；禁止在任务循环里 `rt_malloc`。
7. 头文件：每个 `.c` 对应 `.h`；对外 API 在 `.h`，内部函数 `static`。
8. 配置：引脚/周期/栈大小集中在 `app_config.h`，不要散落魔法数。
9. msh：每个模块至少一个只读/诊断命令；命令不能破坏正在运行的状态机。
10. 提交：一个 Step 一个 commit；应用结构改动先更新本文档。

---

## 7. Step 2 的落地范围

Step 2 只实现架构骨架 + 板载基础：

- `app_main`、`app_ipc`、`app_config`、`app_types`；
- `board_led`、`board_key`、`board_buzzer`；
- `task_sensor`（按键扫描 + LED 指示 + 心跳消息）；
- `task_supervisor`（健康位 + IWDG）；
- `svc_storage`（FAL + EasyFlash 参数）；
- `svc_watchdog`（可被 supervisor 调用）；
- `msh_board`、`msh_cfg`；
- `port/fal_cfg.h`、`port/ef_fal_port.c`；
- 启用 `RT_USING_WDT`、`BSP_USING_WDT`、`BSP_USING_ON_CHIP_FLASH`、`RT_USING_FAL`、`PKG_USING_EASYFLASH`、`RT_USING_ULOG`。

Step 3/4/5 只加 `drivers/`、`services/`、`tasks/`、`shell/` 中的文件，不改 `app_main` 的主结构。

---

## 8. 验收

Step 2 完成后：

```text
msh > led 0 on
msh > led 0 off
msh > key
msh > beep 100
msh > cfg
msh > cfg set srv_host 192.168.1.100
msh > cfg save
msh > cfg reset
```

- 4 LED 独立控制；
- 4 KEY 按下/释放有日志，LED 联动；
- `ps` 看到 sensor/supervisor；
- `free` 无持续下降；
- 按键/采样任务停住时，IWDG 能复位；
- 参数保存后断电重启不丢。
