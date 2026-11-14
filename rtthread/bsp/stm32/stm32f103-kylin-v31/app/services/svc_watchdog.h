#ifndef SVC_WATCHDOG_H__
#define SVC_WATCHDOG_H__

#include <rtthread.h>

int svc_watchdog_init(void);
int svc_watchdog_feed(void);

#endif /* SVC_WATCHDOG_H__ */
