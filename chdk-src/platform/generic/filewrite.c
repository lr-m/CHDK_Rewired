#include "stdlib.h" // for NULL

#include "remotecap_core.h"
#include "bend_tag.h"
static int ignore_current_write=0; //used by the platform routine to check whether to write the current file
#ifdef CAM_DRYOS
static long fwt_bytes_written = 0; // need to track this independently of ptp
volatile int current_write_ignored=0; //needed to prevent nasty surprises with the current override code
#endif //CAM_DRYOS

/*
handle camera specific MAX_CHUNKS_FOR_FWT, camp_ptp_data_chunk
get address and size of chunk N
returns NULL addr and zero size when max chunks reached
*/
static cam_ptp_data_chunk *file_chunks;
#ifdef CAM_FILEWRITETASK_MULTIPASS
static int file_curr_session_chunk;
#ifdef CAM_FILEWRITETASK_SEEKS
static int file_bytes_left;
static int fwt_file_offset;
static int file_full_size;
#else
static int file_last_session;
#endif
#endif

/*
called by ptp task (via remotecap.c code) to fetch chunks of file data
*/
int filewrite_get_file_chunk(char **addr,int *size,unsigned n,int *pos) {
    *pos=-1;
#if !defined(CAM_FILEWRITETASK_MULTIPASS)
    // TODO null should probably be an error
    if (n >= MAX_CHUNKS_FOR_FWT || file_chunks == NULL) {
        *addr=(char *)0xFFFFFFFF; // signals last chunk
        *size=0;
        return REMOTECAP_FWT_CHUNK_STATUS_LAST; // last chunk
    }
    *addr=(char *)file_chunks[n].address;
    *size=file_chunks[n].length;
    if (n < MAX_CHUNKS_FOR_FWT-1) {
        if (file_chunks[n+1].length==0) {
            return REMOTECAP_FWT_CHUNK_STATUS_LAST; // last chunk
        }
        return REMOTECAP_FWT_CHUNK_STATUS_MORE; // not last chunk
    }
    return REMOTECAP_FWT_CHUNK_STATUS_LAST; // last chunk
#else
    if ( file_chunks == NULL ) { //do we have a valid queue?
        int m=50;
        while (m>0) { //wait for at most 500ms
            _SleepTask(10);
            m--;
            if ( file_chunks != NULL ) break;
        }
        if ( file_chunks == NULL ) { //timeout, error
            *addr=(char *)0;
            *size=0;
            return REMOTECAP_FWT_CHUNK_STATUS_LAST;
        }
    }
    *addr=(char *)file_chunks[file_curr_session_chunk].address;
    *size=file_chunks[file_curr_session_chunk].length;
#ifdef CAM_FILEWRITETASK_SEEKS
    if ( n == 0 ) { // first chunk for this shot
        file_bytes_left = file_full_size;
    }
    file_bytes_left -= *size;
    if (file_curr_session_chunk == 0) {
        *pos=fwt_file_offset; //only post file offset for the first chunk in the current queue
    }
#else
    (void)n;
    if ((file_curr_session_chunk==0) && (file_last_session)) {
        *pos=0; //only post file offset for the first chunk in the last queue
    }
#endif
    file_curr_session_chunk++;
#ifdef CAM_FILEWRITETASK_SEEKS
    if (file_bytes_left>0) {
        if ( file_curr_session_chunk < MAX_CHUNKS_FOR_FWT ) {
            if (file_chunks[file_curr_session_chunk].length==0) { //last chunk of the current queue
                return REMOTECAP_FWT_CHUNK_STATUS_SESS_LAST;
            }
            return REMOTECAP_FWT_CHUNK_STATUS_MORE; //not last
        }
        else {
            return REMOTECAP_FWT_CHUNK_STATUS_SESS_LAST;
        }
    }
#else
    if ( file_curr_session_chunk < MAX_CHUNKS_FOR_FWT ) {
        if (file_chunks[file_curr_session_chunk].length==0) { //last chunk of the current queue
            if (file_last_session) {
                return REMOTECAP_FWT_CHUNK_STATUS_LAST;
            }
            else {
                return REMOTECAP_FWT_CHUNK_STATUS_SESS_LAST;
            }
        }
        return REMOTECAP_FWT_CHUNK_STATUS_MORE; //not last
    }
    else {
        if (file_last_session) {
            return REMOTECAP_FWT_CHUNK_STATUS_LAST;
        }
        else {
            return REMOTECAP_FWT_CHUNK_STATUS_SESS_LAST;
        }
    }
#endif
    return REMOTECAP_FWT_CHUNK_STATUS_LAST; //last
#endif //CAM_FILEWRITETASK_MULTIPASS
}

void filewrite_set_discard_file(int state) {
    ignore_current_write = state;
}

void filewrite_main_hook(fwt_data_struct *fwt_data)
{
#ifdef CAM_FILEWRITETASK_MULTIPASS
    file_curr_session_chunk = 0;
#ifdef CAM_FILEWRITETASK_SEEKS
    // don't forget to #define FWT_SEEKMASK, FWT_MUSTSEEK
    /*
     * below information is only valid when this routine is called
     *
     * seek flag is at offset 0x50 for DryOS r50
     * 2: seek is needed
     * if not 2, seek is only performed when file_offset is not 0
     *
     * seek flag is at offset 0x4c for DryOS r51
     * if (seek_flag & 0x40) == 0x40 then seek is needed
     * otherwise, seek is only performed when file_offset is not 0
     */
    if ( ((fwt_data->seek_flag & FWT_SEEKMASK) == FWT_MUSTSEEK) || (fwt_data->file_offset != 0) ) {
        fwt_file_offset = fwt_data->file_offset;
    }
    else {
        fwt_file_offset = -1; // no seek needed
    }
    file_full_size = fwt_data->full_size;
#else
    /*
     * below information is only valid when this routine is called
     *
     * we need to watch the file open flags to determine whether the file data arrives in multiple passes
     * if the flag isn't there, it's the last (or only) pass
     */
    if (fwt_data->oflags & OFLAG_NOFLUSH) {
        file_last_session = 0;
    }
    else {
        file_last_session = 1;
    }
#endif // CAM_FILEWRITETASK_SEEKS
#endif // CAM_FILEWRITETASK_MULTIPASS

    file_chunks = &(fwt_data->pdc[0]);

    // Remember which file this is, for bend_tag_note_file_closed() below. The
    // name is only available here, at the open stage; fwt_close() is not given
    // it. "A/DCIM/100CANON/IMG_1234.JPG" - the extension is at [24], which is
    // what the CR2 test just below has always relied on.
#ifdef CAM_BEND_TAG_INJECT
    bend_tag_note_file_open(fwt_data->name);
#endif

#ifdef CAM_HAS_CANON_RAW
    // for raw enabled cameras, get format from extension
    // A/DCIM/100CANON/IMG_1234.CR2
    if(strcmp(".CR2",&(fwt_data->name[24])) == 0) {
        remotecap_fwt_craw_available();
    } else {
        remotecap_fwt_jpeg_available();
    }
#else
    remotecap_fwt_jpeg_available();
#endif
    file_chunks=NULL;
}

// called from filewrite when the hook has returned for the last time for a given file
// to clear ignored write and tell remotecap raw hook it's ok to proceed to next shot
// returns whether the write was ignored, for simpler asm code on the cams that need to switch filename
int filewrite_file_complete(void) {
    if(ignore_current_write) {
#ifdef CAM_DRYOS
#ifdef CAM_FILEWRITETASK_SEEKS
        if (fwt_bytes_written < file_full_size) {
            return 1;
        }
#elif defined(CAM_FILEWRITETASK_MULTIPASS)
        if (!file_last_session) {
            return 1;
        }
#endif // CAM_FILEWRITETASK_SEEKS, CAM_FILEWRITETASK_MULTIPASS
        current_write_ignored=0;
        fwt_bytes_written = 0;
#endif // CAM_DRYOS
        remotecap_fwt_file_complete();
        return 1;
    }
    return 0;
}

#ifdef CAM_FILEWRITETASK_MULTIPASS
void remotecap_fwt_chunks_done() {
    file_chunks=NULL;
}
#endif // CAM_FILEWRITETASK_MULTIPASS

// wrapper functions for use in filewritetask
#ifdef CAM_DRYOS
int fwt_open(const char *name, int flags, int mode) {
    if (!ignore_current_write) {
        return _Open(name, flags, mode);
    }
    current_write_ignored=1;
    return 255; // fake, invalid file descriptor
}

int fwt_write(int fd, const void *buffer, long nbytes) {
    if (!current_write_ignored) {
        // Slip the bend recipe in as the picture is written, rather than
        // rewriting the whole file afterwards to put it at the front. See the
        // long note in core/bend_tag.c. Returns NULL unless a tag is pending
        // for exactly this file and this is its first chunk.
#ifdef CAM_BEND_TAG_INJECT
        {
            const unsigned char *p = (const unsigned char *)buffer;
            if (nbytes >= 2 && p[0] == 0xff && p[1] == 0xd8) {
                const unsigned char *hdr = 0; int hdrlen = 0;
                const char *txt = 0;          int txtlen = 0;
                if (bend_tag_segment(&hdr, &hdrlen, &txt, &txtlen)) {
                    // SOI, our comment header, its text, then the rest of
                    // Canon's chunk. Canon is told it wrote what it asked to
                    // write; the file is simply longer, which is what inserting
                    // bytes means.
                    if (_Write(fd, p, 2) == 2 &&
                        _Write(fd, hdr, hdrlen) == hdrlen &&
                        _Write(fd, txt, txtlen) == txtlen) {
                        bend_tag_segment_written();
                        _Write(fd, p + 2, nbytes - 2);
                        return (int)nbytes;
                    }
                    // Partial header write - fall through and let Canon's own
                    // write run. The picture matters more than the tag.
                }
            }
        }
#endif
        return _Write(fd, buffer, nbytes);
    }
    fwt_bytes_written += nbytes;
    return (int)nbytes; // "everything's written"
}

#ifdef CAM_FILEWRITETASK_SEEKS
int fwt_lseek(int fd, long offset, int whence) {
    if (!current_write_ignored) {
        return _lseek(fd, offset, whence);
    }
    return (int)offset; // "file position set to the requested value"
}
#endif // CAM_FILEWRITETASK_SEEKS

int fwt_close (int fd) {
    if (!filewrite_file_complete()) {
        int ret = _Close(fd);
        //imagesavecomplete=1;
        // Canon has finished with this file and let go of it. This is the
        // signal the bend tagger needs and it is the whole reason it no longer
        // has to poll: opening the picture before this point does not "check
        // whether it is ready", it blocks on the lock Canon is holding, and
        // doing that from spytask stalls everything CHDK draws for the length
        // of the save. One store, no I/O - see core/bend_tag.c.
#ifdef CAM_BEND_TAG_INJECT
        bend_tag_note_file_closed();
#endif
        fwt_bytes_written = 0;
        return ret;
    }
    return 0;
}
#else // ifdef CAM_DRYOS
/*
 * helper function for VxWorks camera
 * to be used when imagesavecomplete handling is needed
 */
int fwt_after_close (int param) {
//    imagesavecomplete=1;
    return param;
}
#endif //CAM_DRYOS
