#ifndef SVC_SENSOR_H__
#define SVC_SENSOR_H__

#include "app_types.h"

int svc_sensor_init(void);
int svc_sensor_sample(app_sensor_sample_t *sample);

#endif /* SVC_SENSOR_H__ */
