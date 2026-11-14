#ifndef DRV_SHT20_H__
#define DRV_SHT20_H__

#include <rtthread.h>

int drv_sht20_init(void);
int drv_sht20_read(rt_int16_t *temp_x10, rt_uint16_t *humi_x10);

#endif /* DRV_SHT20_H__ */
