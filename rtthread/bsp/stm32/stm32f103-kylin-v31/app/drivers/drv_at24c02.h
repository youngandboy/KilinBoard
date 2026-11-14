#ifndef DRV_AT24C02_H__
#define DRV_AT24C02_H__

#include <rtthread.h>

int drv_at24c02_init(void);
int drv_at24c02_read(rt_uint8_t mem_addr, rt_uint8_t *buf, rt_size_t len);
int drv_at24c02_write(rt_uint8_t mem_addr, const rt_uint8_t *buf, rt_size_t len);

#endif /* DRV_AT24C02_H__ */
