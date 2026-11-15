#ifndef BOOT_YMODEM_H__
#define BOOT_YMODEM_H__

#include <stdint.h>

/* Receive one Ymodem file into OTA_DOWNLOAD_BASE. Returns 0 on success and
 * stores the number of received bytes in *file_size. */
int boot_ymodem_receive(uint32_t *file_size);

#endif /* BOOT_YMODEM_H__ */
