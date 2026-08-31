#include "ptp.h"
#include "platform.h"
#include "conf.h"
#include "remotecap_core.h"
#include "module_load.h"
#include "modules.h"
#include "raw.h"
#include "cachebit.h"

#define HOOK_WAIT_MAX_DEFAULT 3000 // timeout for remotecap hooks, in 10ms sleeps = 30 sec
static int hook_wait_max=HOOK_WAIT_MAX_DEFAULT;

static int available_image_data=0; // type of data available

static int pending_image_data=0; // data types requested but not yet transferred in current shot

static int target_file_num; // file number captured in raw hook

static ptp_data_chunk raw_chunk;
static ptp_data_chunk dng_hdr_chunk;

// raw subimage range
static int startline=0;
static int linecount=0;

#ifdef CAM_HAS_FILEWRITETASK_HOOK
static int fwt_curr_chunk;
static int fwt_last_status;
static int fwt_current_type; // data type of current filewrite file, for cams with raw and jpeg
static int fwt_expect_file_count; // number of filewrite files expected (for cameras with native raw+jpeg)
#ifdef CAM_FILEWRITETASK_MULTIPASS
static int fwt_session_wait; // should the current invocation of the filewrite hook block
#endif //CAM_FILEWRITETASK_MULTIPASS
#endif //CAM_HAS_FILEWRITETASK_HOOK

/*
set hook timeout in ms
*/
void remotecap_set_timeout(int timeout)
{
    if(timeout <= 0) {
        hook_wait_max = HOOK_WAIT_MAX_DEFAULT;
    } else {
        hook_wait_max = timeout/10;
    }
}

void remotecap_is_ready(int *available_type,int *file_num) {
    if ( camera_info.remotecap.file_target ) {
        *available_type = available_image_data;
        if(available_image_data) {
            *file_num = target_file_num;
        } else {
            *file_num = 0;
        }
    }
    else {
        *available_type = PTP_CHDK_CAPTURE_NOTSET;
        *file_num = 0;
    }
}

// could | in if we supported multiple types available at the same time
static void remotecap_set_available_data_type(int type)
{
    available_image_data = type;
}

// free hooks, reset target and available
static void remotecap_reset(void) {
    camera_info.remotecap.file_target=0;
    remotecap_set_available_data_type(0);
    pending_image_data=0;
    // TODO do we need remotecap_fwt_chunks_done() ?
}

//called to activate or deactive hooks
int remotecap_set_target( int type, int lstart, int lcount )
{
    // fail if invalid / unsupported type requested,
    // or current mode cannot support requested types
    if ((type & ~camera_info.remotecap.target_support)
        || !camera_info.state.mode_rec
        || ((type & PTP_CHDK_CAPTURE_RAW) && !is_raw_possible())
#ifdef CAM_HAS_CANON_RAW
        || ((type & PTP_CHDK_CAPTURE_CRAW) && !(shooting_get_canon_image_format() & SHOOTING_CANON_FMT_RAW))
        || ((type & PTP_CHDK_CAPTURE_JPG) && !(shooting_get_canon_image_format() & SHOOTING_CANON_FMT_JPG))
#endif
#if defined(CAM_FILEWRITETASK_MULTIPASS) && !defined(CAM_FILEWRITETASK_SEEKS)
    // other drive modes do not work on these cams currently
        || (shooting_get_drive_mode() != 0)
#endif
        ) {
        remotecap_reset();
        return 0;
    }
    // clear requested
    if(type==0) {
        remotecap_reset();
        return 1;
    }
    // invalid range, return error
    if(lstart<0 || lstart>CAM_RAW_ROWS-1 || lcount<0 || lcount+lstart>CAM_RAW_ROWS) {
        remotecap_reset();
        return 0;
    }
    // default lcount = to end of buffer
    if(lcount == 0) {
        lcount = CAM_RAW_ROWS - lstart;
    }
    camera_info.remotecap.file_target=type;
    startline=lstart;
    linecount=lcount;
    return 1;
}

/*
don't allow unloading DNG module if target is DNG
not optimal since it could lead to loading/unloading every shot
could make logic more sophisticated later
*/
int remotecap_using_dng_module(void) {
    return (camera_info.remotecap.file_target & PTP_CHDK_CAPTURE_DNGHDR) != 0;
}

/*
return 0 if timed out, otherwise non-zero
called from tasks that need to wait for data to be read
*/
static int remotecap_wait(int datatype) {
    int wait = hook_wait_max;

    remotecap_set_available_data_type(datatype);

    while (wait && (available_image_data & datatype)) {
        msleep(10);
        wait--;
    }
    return wait;
}

// called from filewrite or spytask when type is fully completed
static void remotecap_type_complete(int type) {
    pending_image_data = (pending_image_data & ~type);
}

void filewrite_set_discard_file(int state);
int filewrite_get_file_chunk(char **addr,unsigned *size, unsigned n, int *pos);

void remotecap_raw_available(char *rawadr) {
    // get file number as early as possible, before blocking
    // but don't set until after so it doesn't change value for remotecap_is_ready
    int next_file_num = get_target_file_num();
/*
ensure raw hook is blocked until any prevous remotecap shot is finished or times out
if prevous times out, remotecap settings will be cleared due to the time out, so no
remotecap will be done, although writes will still be skipped
wait == 0 timeout shouldn't get hit here unless the script is fiddling with the
timeout value, but it ensures that we don't block indefinitely.
*/
    int wait = hook_wait_max;
    while (wait && pending_image_data) {
        msleep(10);
        wait--;
    }
    if(wait == 0) {
        remotecap_reset();
    }
    pending_image_data = camera_info.remotecap.file_target;
    target_file_num = next_file_num;
// TODO technically this could probably wait until after the raw stuff is done,
// provided the actual chunks are transmitted
// TODO this should probably just be noop if hook doesn't exist
#ifdef CAM_HAS_FILEWRITETASK_HOOK
    filewrite_set_discard_file(1);
    fwt_curr_chunk=0; //needs to be done here
#ifdef CAM_HAS_CANON_RAW
    if(shooting_get_canon_image_format() == (SHOOTING_CANON_FMT_RAW | SHOOTING_CANON_FMT_JPG)) {
        fwt_expect_file_count = 2;
    } else {
        fwt_expect_file_count = 1;
    }
#else
    fwt_expect_file_count = 1;
#endif
#endif //CAM_HAS_FILEWRITETASK_HOOK
    if (camera_info.remotecap.file_target & PTP_CHDK_CAPTURE_DNGHDR) {
        started();
        libdng->create_dng_header_for_ptp(&dng_hdr_chunk);

        if(!remotecap_wait(PTP_CHDK_CAPTURE_DNGHDR)) {
            remotecap_reset();
        }
        libdng->free_dng_header_for_ptp();
        remotecap_type_complete(PTP_CHDK_CAPTURE_DNGHDR);
        finished();
    }

    if(!(camera_info.remotecap.file_target & PTP_CHDK_CAPTURE_RAW)) {
        return;
    }

    started();

    raw_chunk.address=(unsigned int)ADR_TO_UNCACHED(rawadr+startline*CAM_RAW_ROWPIX*CAM_SENSOR_BITS_PER_PIXEL/8);
    raw_chunk.length=linecount*CAM_RAW_ROWPIX*CAM_SENSOR_BITS_PER_PIXEL/8;

    if(!remotecap_wait(PTP_CHDK_CAPTURE_RAW)) {
        remotecap_reset();
    }
    remotecap_type_complete(PTP_CHDK_CAPTURE_RAW);

    finished();
}

#ifdef CAM_HAS_FILEWRITETASK_HOOK
// called from filewrite when file is complete and remotecap is enabled, regardless of whether type is target
void remotecap_fwt_file_complete(void) {
    remotecap_type_complete(fwt_current_type); // clearing non-target type should be harmless...
    // possible this could be reset on error, or not set in mode with broken raw hook
    if(fwt_expect_file_count) {
        fwt_expect_file_count--;
    }
    if(fwt_expect_file_count == 0) {
        filewrite_set_discard_file(0);
    }
}

/*
called from filewrite hook to notify code new file data is available
*/
static void remotecap_fwt_file_available(void) {
    if(!(camera_info.remotecap.file_target & fwt_current_type)) {
        return;
    }
#ifdef CAM_FILEWRITETASK_MULTIPASS
    // need custom wait code here to resume if jpeg queue is done
    int wait = hook_wait_max;

    fwt_session_wait = 1;
    remotecap_set_available_data_type(fwt_current_type);

    while (wait && fwt_session_wait && (available_image_data & fwt_current_type)) {
        msleep(10);
        wait--;
    }
    if(wait==0) {
        remotecap_reset();
    }

#else
    if(!remotecap_wait(fwt_current_type)) {
        remotecap_reset();
    }
#endif
}

void remotecap_fwt_jpeg_available(void) {
    fwt_current_type = PTP_CHDK_CAPTURE_JPG;
    remotecap_fwt_file_available();
}

#ifdef CAM_HAS_CANON_RAW
void remotecap_fwt_craw_available(void) {
    fwt_current_type = PTP_CHDK_CAPTURE_CRAW;
    remotecap_fwt_file_available();
}
#endif

#endif // CAM_HAS_FILEWRITETASK_HOOK

// called by ptp code to get next chunk address/size for the format (fmt) that is being currently worked on
// returns REMOTECAP_CHUNK_STATUS MORE, ERROR or LAST
int remotecap_get_data_chunk( int fmt, char **addr, unsigned int *size, int *pos )
{
    int status = REMOTECAP_CHUNK_STATUS_LAST; // default = no more chunks
    *pos = -1; // default = sequential

    switch (fmt & camera_info.remotecap.file_target & available_image_data)
    {
        case PTP_CHDK_CAPTURE_RAW: //raw
            *addr=(char*)raw_chunk.address;
            *size=raw_chunk.length;
            break;
#ifdef CAM_HAS_FILEWRITETASK_HOOK
        case PTP_CHDK_CAPTURE_CRAW: //canon raw
        case PTP_CHDK_CAPTURE_JPG: //jpeg
            fwt_last_status = filewrite_get_file_chunk(addr,size,fwt_curr_chunk,pos);
            fwt_curr_chunk+=1;
            if(fwt_last_status != REMOTECAP_FWT_CHUNK_STATUS_LAST) {
                status = REMOTECAP_CHUNK_STATUS_MORE;
            }
#ifdef CAM_HAS_CANON_RAW
            else {
                fwt_curr_chunk=0;
            }
#endif
            break;
#endif
        case PTP_CHDK_CAPTURE_DNGHDR: // dng header
            *addr=(char*)dng_hdr_chunk.address;
            *size=dng_hdr_chunk.length;
            break;
        default:
            /*
             * attempting to get an unsupported, unavailable or not requested format
             * will free all hooks, deactiveate remotecap, and return error status
             */
            *addr=NULL;
            *size=0;
    }
    if(*addr == NULL) {
        remotecap_reset();
        status = REMOTECAP_CHUNK_STATUS_ERROR;
    }

    return status;
}

int remotecap_send_complete(int rcgd_status, int type) {
    // timeout or canceled: the data type we were sending is no longer available
    int timeout_flag = (available_image_data != type);
    if(rcgd_status == REMOTECAP_CHUNK_STATUS_LAST) {
        // currently only one data type can be available at a time
        remotecap_set_available_data_type(0);
    }
#ifdef CAM_FILEWRITETASK_MULTIPASS
    else if(type == PTP_CHDK_CAPTURE_JPG && fwt_last_status == REMOTECAP_FWT_CHUNK_STATUS_SESS_LAST) {
        remotecap_fwt_chunks_done(); // make file_chunks NULL, immediately
        fwt_session_wait = 0;
    }
#endif
    if((rcgd_status == REMOTECAP_CHUNK_STATUS_ERROR) || timeout_flag) {
        return 0;
    }
    // else more chunks of current type, no action needed
    return 1;
}
