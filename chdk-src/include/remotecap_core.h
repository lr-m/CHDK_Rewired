#ifndef REMOTECAP_CORE_H
#define REMOTECAP_CORE_H
#include "remotecap.h"
/*
 * remotecap functions not exported to modules
 */
// called by ptp to get currently available type and image number
void remotecap_is_ready(int *available_type,int *image_num);
// call from task hooks to notify remotecap of data
void remotecap_raw_available(char *rawadr);
void remotecap_fwt_jpeg_available(void);
void remotecap_fwt_craw_available(void);
// called by ptp code to get next chunk address/size for the format (fmt) that is being currently worked on
int remotecap_get_data_chunk( int fmt, char **addr, unsigned int *size, int *pos );
// called by ptp code to notify remotecap that chunk send is complete
// returns false if ptp call should return an error, otherwise true
int remotecap_send_complete(int rcgd_status,int type);
// sets file_chunks to NULL for DryOS r50+
void remotecap_fwt_chunks_done(void);
// called from filewrite when file is complete and remotecap is enabled, regardless of whether type is target
void remotecap_fwt_file_complete(void);

// status for remotecap_get_data_chunk
#define REMOTECAP_CHUNK_STATUS_ERROR 0
#define REMOTECAP_CHUNK_STATUS_MORE 1
#define REMOTECAP_CHUNK_STATUS_LAST 2

// status for filewrite_get_file_chunk
#define REMOTECAP_FWT_CHUNK_STATUS_MORE 1 // more chunks in current session
#define REMOTECAP_FWT_CHUNK_STATUS_LAST 2 // last chunk for all session
#define REMOTECAP_FWT_CHUNK_STATUS_SESS_LAST 3 // last chunk in current session (only for FILEWRITETASK_SEEKS)

#endif
