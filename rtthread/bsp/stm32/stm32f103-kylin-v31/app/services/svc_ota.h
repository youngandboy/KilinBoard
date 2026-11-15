#ifndef SVC_OTA_H__
#define SVC_OTA_H__

#include "app_types.h"
#include "ota_layout.h"

int svc_ota_status(ota_meta_t *meta);
int svc_ota_request(void);

#endif /* SVC_OTA_H__ */
