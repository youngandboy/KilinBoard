#ifndef SVC_NET_H__
#define SVC_NET_H__

#include <rtthread.h>

int svc_net_init(void);
int svc_net_is_ready(void);
void svc_net_reset(void);

int svc_net_socket_connect(const char *host, rt_uint16_t port);
int svc_net_server_start(rt_uint16_t port);
int svc_net_server_accept(void);
int svc_net_socket_send(const void *buf, rt_size_t len);
int svc_net_socket_recv(void *buf, rt_size_t len);
void svc_net_socket_close(void);
int svc_net_socket_is_connected(void);

#endif /* SVC_NET_H__ */
