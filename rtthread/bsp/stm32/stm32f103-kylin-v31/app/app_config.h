#ifndef APP_CONFIG_H__
#define APP_CONFIG_H__

#include <rtthread.h>
#include <drv_gpio.h>

/* --------------------------------------------------------------------------
 * Board pins (kyLin-V3.1)
 * -------------------------------------------------------------------------- */
#define APP_LED_RED_PIN      GET_PIN(C, 7)
#define APP_LED_GREEN_PIN    GET_PIN(C, 8)
#define APP_LED_YELLOW_PIN   GET_PIN(A, 12)
#define APP_LED_BLUE_PIN     GET_PIN(C, 10)
#define APP_LED_COUNT        4

#define APP_KEY1_PIN         GET_PIN(C, 13)
#define APP_KEY2_PIN         GET_PIN(C, 12)
#define APP_KEY3_PIN         GET_PIN(C, 11)
#define APP_KEY4_PIN         GET_PIN(D, 2)
#define APP_KEY_COUNT        4

#define APP_BUZZER_PIN       GET_PIN(B, 3)

/* --------------------------------------------------------------------------
 * I2C / sensors
 * -------------------------------------------------------------------------- */
#define APP_I2C_BUS_NAME        "swi2c1"
#define APP_AT24C02_ADDR        0x50
#define APP_AT24C02_PAGE_SIZE   8
#define APP_SHT20_ADDR          0x40
#define APP_LIGHT_ADC_DEV       "adc1"
#define APP_LIGHT_ADC_CHANNEL   13

/* --------------------------------------------------------------------------
 * ESP8266 (USART2)
 * -------------------------------------------------------------------------- */
#define APP_ESP8266_UART        "uart2"
#define APP_ESP8266_RESET_PIN   GET_PIN(A, 0)
#define APP_ESP8266_BAUD        115200

/* --------------------------------------------------------------------------
 * Tasks
 * -------------------------------------------------------------------------- */
#define APP_TASK_SENSOR_STACK          1024
#define APP_TASK_SENSOR_PRIORITY       12
#define APP_TASK_SENSOR_TICK           10

#define APP_TASK_SUPERVISOR_STACK      768
#define APP_TASK_SUPERVISOR_PRIORITY   11
#define APP_TASK_SUPERVISOR_TICK       10

#define APP_TASK_NET_STACK             2048
#define APP_TASK_NET_PRIORITY          14
#define APP_TASK_NET_TICK              10

/* --------------------------------------------------------------------------
 * Timing
 * -------------------------------------------------------------------------- */
#define APP_KEY_SCAN_MS            10
#define APP_KEY_DEBOUNCE_COUNT     3
#define APP_SENSOR_PERIOD_MS       1000
#define APP_SUPERVISOR_PERIOD_MS   1000
#define APP_WDT_TIMEOUT_S          5

/* Network reconnect policy. Backoff is reset after a successful connect. */
#define APP_NET_LINK_POLL_MS       1000
#define APP_NET_RECONNECT_MIN_MS   1000
#define APP_NET_RECONNECT_MAX_MS   30000
#define APP_NET_TX_TIMEOUT_MS      1000

/* --------------------------------------------------------------------------
 * IPC
 * -------------------------------------------------------------------------- */
#define APP_MQ_DEPTH               16

/* --------------------------------------------------------------------------
 * Persistent configuration
 * -------------------------------------------------------------------------- */
#define APP_CFG_SSID_MAX           32
#define APP_CFG_PWD_MAX            64
#define APP_CFG_HOST_MAX           64

#define APP_CFG_DEFAULT_SSID       ""
#define APP_CFG_DEFAULT_PWD        ""
#define APP_CFG_DEFAULT_HOST       "192.168.1.100"
#define APP_CFG_DEFAULT_PORT       8080
#define APP_CFG_DEFAULT_PERIOD_S   5

#endif /* APP_CONFIG_H__ */
