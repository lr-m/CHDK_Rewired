#ifndef FIRMWARE_CRC_TYPES_H
#define FIRMWARE_CRC_TYPES_H

// Firmware checksum data
typedef struct {
    const char *start;
    unsigned size;
    unsigned crc;
} firmware_crc_block_t;

typedef struct {
    const char *canon_sub;
    const firmware_crc_block_t *blocks;
} firmware_crc_sub_t;

typedef struct {
    const char *firmware_ver_ptr;
    const firmware_crc_sub_t *subs;
    const unsigned sub_count;
    const unsigned block_count;
} firmware_crc_desc_t;
#endif
