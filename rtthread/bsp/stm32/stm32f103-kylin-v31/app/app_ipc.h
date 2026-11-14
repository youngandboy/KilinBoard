#ifndef APP_IPC_H__
#define APP_IPC_H__

#include "app_types.h"

int app_ipc_init(void);

rt_err_t app_mq_send(const app_msg_t *msg);
rt_err_t app_mq_recv(app_msg_t *msg, rt_int32_t timeout);

rt_err_t app_event_send(rt_uint32_t bits);
rt_uint32_t app_event_recv(rt_uint32_t set, rt_uint8_t option, rt_int32_t timeout);

#endif /* APP_IPC_H__ */
