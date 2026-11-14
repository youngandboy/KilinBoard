#ifndef SVC_NET_H__
#define SVC_NET_H__

#include <rtthread.h>

int svc_net_init(void);
int svc_net_is_ready(void);
void svc_net_reset(void);

#endif /* SVC_NET_H__ */
