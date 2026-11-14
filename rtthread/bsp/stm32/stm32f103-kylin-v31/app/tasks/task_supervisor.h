#ifndef TASK_SUPERVISOR_H__
#define TASK_SUPERVISOR_H__

#include <rtthread.h>

int task_supervisor_start(void);
void task_supervisor_set_feed_enabled(int enable);

#endif /* TASK_SUPERVISOR_H__ */
