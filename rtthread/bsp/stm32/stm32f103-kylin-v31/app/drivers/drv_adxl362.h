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

#endif /* DRV_ADXL362_H__ */
