#include "platform.h"

#ifdef CAM_MOVIEREC_NEWSTYLE

#include "conf.h"
#include "clock.h"

static int bitrate_multi_a, bitrate_multi_b;
static int mbitrate_multi_a, mbitrate_multi_b;
static unsigned int recorded_kb;

/*
 * functions called from core
 */

void change_video_tables(int a, int b) {
    bitrate_multi_a = a;
    bitrate_multi_b = b;
}
void change_video_minbitrate(int a, int b) {
    mbitrate_multi_a = a;
    mbitrate_multi_b = b;
}
unsigned int get_video_recorded_size_kb() {
    return recorded_kb;
}

/*
 * functions called from platform
 */

// as of writing, factory bitrates are set to approx. cbr;
// in all known cases, high-mid and mid-low is '100'
// that is the source of the magic value below
void  set_quality(int *struc){ // struc: addr of fw struct on stack
    int low = *(struc+BR_LOW_OFS/4);
    int mid = *(struc+BR_MID_OFS/4);
    int hgh = *(struc+BR_HI_OFS/4);
    if (conf.video_mode == 1) { // CBR mode
        hgh = hgh * bitrate_multi_a / bitrate_multi_b;
        if (hgh > MAX_VIDEO_BITRATE) hgh = MAX_VIDEO_BITRATE;
        mid = (hgh > 400) ? hgh-100 : hgh-(hgh>>2);
        low = (mid > 400) ? mid-100 : mid-(mid>>2);
    }
    else if (conf.video_mode == 2) { // VBR HI mode
        hgh = hgh * bitrate_multi_a / bitrate_multi_b;
        if (hgh > MAX_VIDEO_BITRATE) hgh = MAX_VIDEO_BITRATE;
        low = hgh * mbitrate_multi_a / mbitrate_multi_b;
        mid = (hgh-low > 400) ? hgh-100 : hgh-((hgh-low)>>2);
    }
    else if (conf.video_mode == 3) { // VBR MID mode
        hgh = hgh * bitrate_multi_a / bitrate_multi_b;
        if (hgh > MAX_VIDEO_BITRATE) hgh = MAX_VIDEO_BITRATE;
        low = hgh * mbitrate_multi_a / mbitrate_multi_b;
        mid = (hgh + low) / 2;
    }
    else if (conf.video_mode == 4) { // VBR LOW mode
        hgh = hgh * bitrate_multi_a / bitrate_multi_b;
        if (hgh > MAX_VIDEO_BITRATE) hgh = MAX_VIDEO_BITRATE;
        low = hgh * mbitrate_multi_a / mbitrate_multi_b;
        mid = (hgh-low > 400) ? low+100 : low+((hgh-low)>>2);
    }
    if ((hgh>0) && (mid>0) && (low>0)) {
        *(struc+BR_LOW_OFS/4) = low;
        *(struc+BR_MID_OFS/4) = mid;
        *(struc+BR_HI_OFS/4) = hgh;
    }
    // we're at start of a recording, init movie data variables
    recorded_kb = 0;
}

int set_movie_time_limit(int limit) {
    // 
    if ((conf.ext_video_time == 1) && (limit <= FW_SHORT_TIME_LIMIT)) {
        // limits below threshold need user permission
        return SHORT_TIME_LIMIT;
    }
    // all other movie kinds get the higher limit
    return LONG_TIME_LIMIT;
}

void bitrate_calc(unsigned int sum) {
    recorded_kb += sum / 1024;
}

#endif // CAM_MOVIEREC_NEWSTYLE