#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "app_config.h"
#include "app_types.h"
#include "drv_adxl362.h"

/* ADXL362 commands and registers. */
#define ADXL_WRITE_REG      0x0A
#define ADXL_READ_REG       0x0B
#define ADXL_READ_FIFO      0x0D

#define ADXL_REG_DEVID_AD   0x00
#define ADXL_REG_PARTID     0x02
#define ADXL_REG_XDATA_L    0x0E
#define ADXL_REG_STATUS     0x0B
#define ADXL_REG_FIFO_L     0x0C
#define ADXL_REG_FIFO_H     0x0D
#define ADXL_REG_SOFT_RESET 0x1F
#define ADXL_REG_FIFO_CTL   0x28
#define ADXL_REG_FIFO_SAMPLES 0x29
#define ADXL_REG_INTMAP1    0x2A
#define ADXL_REG_INTMAP2    0x2B
#define ADXL_REG_FILTER_CTL 0x2C
#define ADXL_REG_POWER_CTL  0x2D

#define ADXL_RESET_KEY      0x52
#define ADXL_PART_ID        0xF2
#define ADXL_DEVID_AD       0xAD

#define ADXL_FILTER_2G_100HZ  0x03
#define ADXL_POWER_MEASURE    0x02
#define ADXL_INTMAP_DATA_READY  0x01
#define ADXL_FIFO_STREAM        0x02
#define ADXL_FIFO_SAMPLES       16

#define ADXL_FIFO_BURST_MAX     32

/* PA4 is the ADXL362 chip select. SPI1 uses PA5/PA6/PA7. */
#define ADXL_CS_LOW()       (GPIOA->BSRR = (1U << (4U + 16U)))
#define ADXL_CS_HIGH()      (GPIOA->BSRR = (1U << 4U))

static int adxl_ready;
static struct rt_mutex adxl_lock;
static volatile rt_uint32_t adxl_irq_total;
static volatile rt_uint32_t adxl_int2_irq_total;
static rt_int16_t adxl_off_x;
static rt_int16_t adxl_off_y;
static rt_int16_t adxl_off_z;

static void adxl_int1_callback(void *args)
{
    (void)args;
    adxl_irq_total++;
}

static void adxl_int2_callback(void *args)
{
    (void)args;
    adxl_int2_irq_total++;
}

static void adxl_spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_SPI1EN;

    /* PA4: output push-pull; PA5/PA7: AF push-pull; PA6: floating input. */
    GPIOA->CRL &= ~(0xFU << 16);
    GPIOA->CRL |= (0x3U << 16);
    GPIOA->CRL &= ~(0xFU << 20);
    GPIOA->CRL |= (0xBU << 20);
    GPIOA->CRL &= ~(0xFU << 24);
    GPIOA->CRL |= (0x4U << 24);
    GPIOA->CRL &= ~(0xFU << 28);
    GPIOA->CRL |= (0xBU << 28);

    ADXL_CS_HIGH();

    SPI1->CR1 = 0;
    /* Master, software NSS, mode 3, 8-bit, 72 MHz / 16 = 4.5 MHz. */
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                SPI_CR1_CPOL | SPI_CR1_CPHA |
                (0x3U << 3) | SPI_CR1_SPE;
}

static rt_uint8_t adxl_xfer(rt_uint8_t value)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0U)
    {
    }
    SPI1->DR = value;
    while ((SPI1->SR & SPI_SR_RXNE) == 0U)
    {
    }
    return (rt_uint8_t)SPI1->DR;
}

static int adxl_read_reg(rt_uint8_t reg, rt_uint8_t *data, rt_size_t len)
{
    rt_size_t i;

    if ((data == RT_NULL) || (len == 0U))
    {
        return APP_ERR_PARAM;
    }

    ADXL_CS_LOW();
    (void)adxl_xfer(ADXL_READ_REG);
    (void)adxl_xfer(reg);
    for (i = 0; i < len; i++)
    {
        data[i] = adxl_xfer(0x00);
    }
    ADXL_CS_HIGH();

    return APP_OK;
}

static int adxl_write_reg(rt_uint8_t reg, rt_uint8_t value)
{
    ADXL_CS_LOW();
    (void)adxl_xfer(ADXL_WRITE_REG);
    (void)adxl_xfer(reg);
    (void)adxl_xfer(value);
    ADXL_CS_HIGH();

    return APP_OK;
}

int drv_adxl362_read_id(rt_uint8_t *devid, rt_uint8_t *partid)
{
    int result = APP_OK;

    if ((devid == RT_NULL) || (partid == RT_NULL))
    {
        return APP_ERR_PARAM;
    }
    if (!adxl_ready)
    {
        return APP_ERR_STATE;
    }

    rt_mutex_take(&adxl_lock, RT_WAITING_FOREVER);
    if (adxl_read_reg(ADXL_REG_DEVID_AD, devid, 1) != APP_OK)
    {
        result = APP_ERR_IO;
    }
    else if (adxl_read_reg(ADXL_REG_PARTID, partid, 1) != APP_OK)
    {
        result = APP_ERR_IO;
    }
    rt_mutex_release(&adxl_lock);
    return result;
}

int drv_adxl362_init(void)
{
    rt_uint8_t devid = 0;
    rt_uint8_t partid = 0;

    adxl_spi_init();
    if (rt_mutex_init(&adxl_lock, "adxl", RT_IPC_FLAG_PRIO) != RT_EOK)
    {
        rt_kprintf("[adxl] mutex init failed\n");
        return APP_ERR;
    }
    adxl_ready = 1;

    adxl_write_reg(ADXL_REG_SOFT_RESET, ADXL_RESET_KEY);
    rt_thread_mdelay(10);

    adxl_read_reg(ADXL_REG_DEVID_AD, &devid, 1);
    adxl_read_reg(ADXL_REG_PARTID, &partid, 1);

    if ((devid != ADXL_DEVID_AD) || (partid != ADXL_PART_ID))
    {
        rt_kprintf("[adxl] id mismatch dev=0x%02x part=0x%02x\n", devid, partid);
        adxl_ready = 0;
        return APP_ERR_IO;
    }

    /* Both INT1 and INT2 expose data-ready; FIFO reads are polled. */
    adxl_write_reg(ADXL_REG_INTMAP1, ADXL_INTMAP_DATA_READY);
    adxl_write_reg(ADXL_REG_INTMAP2, ADXL_INTMAP_DATA_READY);
    rt_pin_mode(APP_ADXL362_INT1_PIN, PIN_MODE_INPUT_PULLDOWN);
    rt_pin_mode(APP_ADXL362_INT2_PIN, PIN_MODE_INPUT_PULLDOWN);
    if (rt_pin_attach_irq(APP_ADXL362_INT1_PIN, PIN_IRQ_MODE_RISING,
                          adxl_int1_callback, RT_NULL) != RT_EOK)
    {
        rt_kprintf("[adxl] int1 attach failed\n");
    }
    else
    {
        rt_pin_irq_enable(APP_ADXL362_INT1_PIN, PIN_IRQ_ENABLE);
    }

    if (rt_pin_attach_irq(APP_ADXL362_INT2_PIN, PIN_IRQ_MODE_RISING,
                          adxl_int2_callback, RT_NULL) != RT_EOK)
    {
        rt_kprintf("[adxl] int2 attach failed\n");
    }
    else
    {
        rt_pin_irq_enable(APP_ADXL362_INT2_PIN, PIN_IRQ_ENABLE);
    }

    /* 2 g range, 100 Hz ODR, then measurement mode. */
    adxl_write_reg(ADXL_REG_FILTER_CTL, ADXL_FILTER_2G_100HZ);
    adxl_write_reg(ADXL_REG_POWER_CTL, ADXL_POWER_MEASURE);
    rt_thread_mdelay(20);

    {
        rt_uint8_t status = 0;
        rt_uint8_t pwr = 0;
        rt_uint8_t filt = 0;

        adxl_read_reg(ADXL_REG_STATUS, &status, 1);
        adxl_read_reg(ADXL_REG_POWER_CTL, &pwr, 1);
        adxl_read_reg(ADXL_REG_FILTER_CTL, &filt, 1);
        rt_kprintf("[adxl] init ok dev=0x%02x part=0x%02x "
                   "status=0x%02x pwr=0x%02x filt=0x%02x\n",
                   devid, partid, status, pwr, filt);
    }
    return APP_OK;
}

static int adxl_read_raw(adxl362_xyz_t *xyz)
{
    rt_uint8_t raw[6];

    if (adxl_read_reg(ADXL_REG_XDATA_L, raw, sizeof(raw)) != APP_OK)
    {
        return APP_ERR_IO;
    }

    xyz->x_mg = (rt_int16_t)(((rt_uint16_t)raw[1] << 8) | raw[0]);
    xyz->y_mg = (rt_int16_t)(((rt_uint16_t)raw[3] << 8) | raw[2]);
    xyz->z_mg = (rt_int16_t)(((rt_uint16_t)raw[5] << 8) | raw[4]);
    return APP_OK;
}

int drv_adxl362_read(adxl362_xyz_t *xyz)
{
    int result = APP_OK;

    if (xyz == RT_NULL)
    {
        return APP_ERR_PARAM;
    }
    if (!adxl_ready)
    {
        return APP_ERR_STATE;
    }

    rt_mutex_take(&adxl_lock, RT_WAITING_FOREVER);
    if (adxl_read_raw(xyz) != APP_OK)
    {
        result = APP_ERR_IO;
    }
    else
    {
        xyz->x_mg = (rt_int16_t)(xyz->x_mg - adxl_off_x);
        xyz->y_mg = (rt_int16_t)(xyz->y_mg - adxl_off_y);
        xyz->z_mg = (rt_int16_t)(xyz->z_mg - adxl_off_z);
    }
    rt_mutex_release(&adxl_lock);
    return result;
}

int drv_adxl362_calibrate(rt_uint16_t samples)
{
    rt_int32_t sum_x = 0;
    rt_int32_t sum_y = 0;
    rt_int32_t sum_z = 0;
    rt_uint16_t i;
    int result = APP_OK;

    if (!adxl_ready)
    {
        return APP_ERR_STATE;
    }
    if (samples == 0U)
    {
        samples = 32U;
    }

    rt_mutex_take(&adxl_lock, RT_WAITING_FOREVER);
    for (i = 0; i < samples; i++)
    {
        adxl362_xyz_t raw;

        if (adxl_read_raw(&raw) != APP_OK)
        {
            result = APP_ERR_IO;
            break;
        }
        sum_x += raw.x_mg;
        sum_y += raw.y_mg;
        sum_z += raw.z_mg;
        rt_thread_mdelay(5);
    }

    if (result == APP_OK)
    {
        adxl_off_x = (rt_int16_t)(sum_x / (rt_int32_t)samples);
        adxl_off_y = (rt_int16_t)(sum_y / (rt_int32_t)samples);
        /* The board is assumed level during calibration: target Z is +1 g. */
        adxl_off_z = (rt_int16_t)((sum_z / (rt_int32_t)samples) - 1000);
    }
    rt_mutex_release(&adxl_lock);
    return result;
}

void drv_adxl362_clear_calibration(void)
{
    adxl_off_x = 0;
    adxl_off_y = 0;
    adxl_off_z = 0;
}

void drv_adxl362_get_offsets(rt_int16_t *x, rt_int16_t *y, rt_int16_t *z)
{
    if (x != RT_NULL) { *x = adxl_off_x; }
    if (y != RT_NULL) { *y = adxl_off_y; }
    if (z != RT_NULL) { *z = adxl_off_z; }
}

rt_uint32_t drv_adxl362_irq_count(void)
{
    return adxl_irq_total;
}

rt_uint32_t drv_adxl362_int2_irq_count(void)
{
    return adxl_int2_irq_total;
}

int drv_adxl362_read_fifo(adxl362_xyz_t *samples, rt_uint32_t capacity, rt_uint32_t *count)
{
    rt_uint8_t count_raw[2];
    rt_uint8_t raw[ADXL_FIFO_BURST_MAX * 6];
    rt_uint32_t fifo_count;
    rt_uint32_t i;

    if ((samples == RT_NULL) || (count == RT_NULL) || (capacity == 0U))
    {
        return APP_ERR_PARAM;
    }
    if (!adxl_ready)
    {
        return APP_ERR_STATE;
    }

    rt_mutex_take(&adxl_lock, RT_WAITING_FOREVER);
    adxl_write_reg(ADXL_REG_FIFO_CTL, ADXL_FIFO_STREAM);
    adxl_write_reg(ADXL_REG_FIFO_SAMPLES, ADXL_FIFO_SAMPLES);
    rt_thread_mdelay(250);

    adxl_read_reg(ADXL_REG_FIFO_L, count_raw, 2);
    fifo_count = ((rt_uint32_t)(count_raw[1] & 0x03U) << 8) | count_raw[0];
    if (fifo_count > capacity)
    {
        fifo_count = capacity;
    }
    if (fifo_count > ADXL_FIFO_BURST_MAX)
    {
        fifo_count = ADXL_FIFO_BURST_MAX;
    }

    if (fifo_count > 0U)
    {
        ADXL_CS_LOW();
        (void)adxl_xfer(ADXL_READ_FIFO);
        for (i = 0; i < (fifo_count * 6U); i++)
        {
            raw[i] = adxl_xfer(0x00);
        }
        ADXL_CS_HIGH();

        for (i = 0; i < fifo_count; i++)
        {
            rt_uint32_t axis;
            rt_int16_t x = 0;
            rt_int16_t y = 0;
            rt_int16_t z = 0;

            for (axis = 0; axis < 3U; axis++)
            {
                const rt_uint8_t *p = &raw[(i * 3U + axis) * 2U];
                rt_uint16_t entry = (rt_uint16_t)p[0] | ((rt_uint16_t)p[1] << 8);
                rt_uint16_t value = entry & 0x0FFFU;
                rt_int16_t signed_value = (value & 0x0800U)
                                        ? (rt_int16_t)(value | 0xF000U)
                                        : (rt_int16_t)value;

                switch (entry >> 14)
                {
                case 1U:
                    y = signed_value;
                    break;
                case 2U:
                    z = signed_value;
                    break;
                case 0U:
                default:
                    x = signed_value;
                    break;
                }
            }

            samples[i].x_mg = (rt_int16_t)(x - adxl_off_x);
            samples[i].y_mg = (rt_int16_t)(y - adxl_off_y);
            samples[i].z_mg = (rt_int16_t)(z - adxl_off_z);
        }
    }

    adxl_write_reg(ADXL_REG_FIFO_CTL, 0);
    adxl_write_reg(ADXL_REG_FIFO_SAMPLES, 0);
    rt_mutex_release(&adxl_lock);
    *count = fifo_count;
    return APP_OK;
}
