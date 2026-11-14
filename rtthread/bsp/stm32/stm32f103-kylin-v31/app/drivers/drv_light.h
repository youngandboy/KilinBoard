#ifndef DRV_LIGHT_H__
#define DRV_LIGHT_H__

#include <rtthread.h>

int drv_light_init(void);
int drv_light_read(rt_uint16_t *millivolt);

#endif /* DRV_LIGHT_H__ */
