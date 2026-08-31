/* glue to get checksums from generated header in platform/sub into core
if actual data is not available due to missing dump or inability to run tools/make-fw-crc.py
define FIRMWARE_CRC_DISABLED in firmware_crc_data.h
*/

#include "firmware_crc_types.h"
#include "firmware_crc_data.h"

#ifdef FIRMWARE_CRC_DISABLED
const firmware_crc_desc_t firmware_crc_desc={
    (const char *)0, // firmware_ver_ptr
    (const firmware_crc_sub_t *)0, // subs
    0,  // sub_count
    0,  // block_count
};
#endif
