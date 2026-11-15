#ifndef DRV_ADXL362_H__
#define DRV_ADXL362_H__

#include <rtthread.h>

typedef struct
{
    rt_int16_t x_mg;
    rt_int16_t y_mg;
    rt_int16_t z_mg;
} adxl362_xyz_t;

int drv_adxl362_init(void);
int drv_adxl362_read(adxl362_xyz_t *xyz);
int drv_adxl362_read_id(rt_uint8_t *devid, rt_uint8_t *partid);
int drv_adxl362_calibrate(rt_uint16_t samples);
void drv_adxl362_clear_calibration(void);
void drv_adxl362_get_offsets(rt_int16_t *x, rt_int16_t *y, rt_int16_t *z);
rt_uint32_t drv_adxl362_irq_count(void);
rt_uint32_t drv_adxl362_int2_irq_count(void);
int drv_adxl362_read_fifo(adxl362_xyz_t *samples, rt_uint32_t capacity, rt_uint32_t *count);

#endif /* DRV_ADXL362_H__ */
