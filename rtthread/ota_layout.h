#ifndef OTA_LAYOUT_H__
#define OTA_LAYOUT_H__

#include <stdint.h>

/* Flash layout for the OTA-capable firmware. */
#define OTA_BOOT_BASE           (0x08000000U)
#define OTA_BOOT_SIZE           (16U * 1024U)
#define OTA_META_BASE           (0x08003800U)
#define OTA_META_SIZE           (2U * 1024U)
#define OTA_APP_BASE            (0x08004000U)
#define OTA_APP_SIZE            (232U * 1024U)
#define OTA_DOWNLOAD_BASE       (0x0803E000U)
#define OTA_DOWNLOAD_SIZE       (232U * 1024U)
#define OTA_EF_BASE             (0x08078000U)
#define OTA_EF_SIZE             (32U * 1024U)

#define OTA_FLASH_PAGE_SIZE     (2U * 1024U)

/* Metadata page at OTA_META_BASE. */
#define OTA_META_MAGIC          (0x4B594D45U) /* "KYME" */
#define OTA_META_STATE_IDLE     0U
#define OTA_META_STATE_REQUEST  1U
#define OTA_META_STATE_DOWNLOAD 2U

typedef struct
{
    uint32_t magic;
    uint32_t state;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t version;
    uint32_t reserved[3];
} ota_meta_t;

/* Container received through Ymodem: header + raw rtthread.bin payload. */
#define OTA_HEADER_MAGIC_SIZE   8U
typedef struct
{
    uint8_t  magic[OTA_HEADER_MAGIC_SIZE];
    uint32_t version;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t header_crc32;
    uint8_t  reserved[8];
} ota_header_t;

static const uint8_t ota_header_magic[OTA_HEADER_MAGIC_SIZE] =
    { 'K', 'Y', 'O', 'T', 'A', '1', 0, 0 };

#endif /* OTA_LAYOUT_H__ */
