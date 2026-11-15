#include <rtthread.h>
#include <board.h>
#include "app_config.h"
#include "app_types.h"
#include "drv_adxl362.h"

/* ADXL362 commands and registers. */
#define ADXL_WRITE_REG      0x0A
#define ADXL_READ_REG       0x0B

#define ADXL_REG_DEVID_AD   0x00
#define ADXL_REG_PARTID     0x02
#define ADXL_REG_XDATA_L    0x0E
#define ADXL_REG_STATUS     0x0B
#define ADXL_REG_SOFT_RESET 0x1F
#define ADXL_REG_FILTER_CTL 0x2C
#define ADXL_REG_POWER_CTL  0x2D

#define ADXL_RESET_KEY      0x52
#define ADXL_PART_ID        0xF2
#define ADXL_DEVID_AD       0xAD

#define ADXL_FILTER_2G_100HZ  0x03
#define ADXL_POWER_MEASURE    0x02

/* PA4 is the ADXL362 chip select. SPI1 uses PA5/PA6/PA7. */
#define ADXL_CS_LOW()       (GPIOA->BSRR = (1U << (4U + 16U)))
#define ADXL_CS_HIGH()      (GPIOA->BSRR = (1U << 4U))

static int adxl_ready;

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
    if ((devid == RT_NULL) || (partid == RT_NULL))
    {
        return APP_ERR_PARAM;
    }
    if (!adxl_ready)
    {
        return APP_ERR_STATE;
    }

    if (adxl_read_reg(ADXL_REG_DEVID_AD, devid, 1) != APP_OK)
    {
        return APP_ERR_IO;
    }
    if (adxl_read_reg(ADXL_REG_PARTID, partid, 1) != APP_OK)
    {
        return APP_ERR_IO;
    }
    return APP_OK;
}

int drv_adxl362_init(void)
{
    rt_uint8_t devid = 0;
    rt_uint8_t partid = 0;

    adxl_spi_init();
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

int drv_adxl362_read(adxl362_xyz_t *xyz)
{
    rt_uint8_t raw[6];

    if (xyz == RT_NULL)
    {
        return APP_ERR_PARAM;
    }
    if (!adxl_ready)
    {
        return APP_ERR_STATE;
    }

    if (adxl_read_reg(ADXL_REG_XDATA_L, raw, sizeof(raw)) != APP_OK)
    {
        return APP_ERR_IO;
    }

    xyz->x_mg = (rt_int16_t)(((rt_uint16_t)raw[1] << 8) | raw[0]);
    xyz->y_mg = (rt_int16_t)(((rt_uint16_t)raw[3] << 8) | raw[2]);
    xyz->z_mg = (rt_int16_t)(((rt_uint16_t)raw[5] << 8) | raw[4]);

    return APP_OK;
}
