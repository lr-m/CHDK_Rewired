#include "camera_info.h"
#include "math.h"
#include "conf.h"
#include "viewport.h"
#include "shooting.h"
#include "modes.h"
#include "lens.h"

//-------------------------------------------------------------------
// AUTO ISO

//////////////////////////////////////////////////////////////////////////////////////////////
// @tsv
// Module below calculate live histogram in same way as OSD histogram
// Difference from shot_histogram family is that live_histogram give answer before shot
// Regular histogram_process cannot be used, because raw non-summarized 0.255 values required
// This module is used in AutoISO2 mechanizm.
//////////////////////////////////////////////////////////////////////////////////////////////

// This uses 30 'Y' values from each row (or seccond row for cameras with 2x height YUV viewports).
// Is summing 30 columns of data a good selection for histogram?

// Define how many viewport bytes to step in each loop iteration. Skip 6 sets of 4 pixels.
#ifdef THUMB_FW
// Digic 6: Each block is 4 bytes (UYVY) 2 Y values
#define HISTO_STEP_SIZE 48
#else
// Each block is 6 bytes (UYVYYY) / 4 Y values
#define HISTO_STEP_SIZE 36
#endif

/*
build histogram of viewport Y values (downsampled by HISTO_STEP_SIZE)
NOTE also used by lua get_live_histo
*/
int live_histogram_read_y(unsigned short *h)
{
#ifdef THUMB_FW
    int vp_width = vid_get_viewport_width_proper() * 2;             // X bytes per row
#else
    int vp_width = vid_get_viewport_width_proper() * 6 / 4;         // X bytes per row
#endif

    int yscale = vid_get_viewport_yscale();   // Y scale factor (2 for 480 line high lv buffer on pre-d6)
    int vp_height_full = vid_get_viewport_height_proper();
    // for large resolutions, increase yscale to avoid overflow
    if(vp_height_full > 480) {
        // minimum 2
        yscale = vp_height_full/240;
    }

    int vp_height = vp_height_full / yscale;      // Number of rows to process

    // account for cases where step does not exactly divide width
    int total = ((vp_width + HISTO_STEP_SIZE/2)/HISTO_STEP_SIZE) * vp_height;

    memset(h, 0, sizeof(unsigned short)*256);

    unsigned char *img = vid_get_viewport_active_buffer();
    if (img)
    {
        int vp_offset = vid_get_viewport_byte_width() * yscale;     // Bytes length of each row (or double row)
        img += vid_get_viewport_image_offset() + 1; // Skip border and move to 1st Y component in each block

        int x, y;
        for (y=0; y<vp_height; y+=1, img+=vp_offset)
        {
            for (x=HISTO_STEP_SIZE/2; x<vp_width; x+=HISTO_STEP_SIZE)
            {
                h[img[x]] += 1;
//                img[x] = img[x+2] = 255;  // Change sampled values on screen for debugging
            }
        }
    } else {
        // if no live buffer, act as if all black
        // should be rare, generally only happens in playback with video selected
        h[0] = total; 
    }

    return total;
}

static int live_histogram_get_range(unsigned short *histo, int total,int from, int to)
{
    if (from < 0) from = 0;
    if (to > 255) to = 255;

    int rv = 0;
    for(; from<=to; from++)
        rv += histo[from];

    return (rv * 100) / total;
}

//-------------------------------------------------------------------

static const int shutter1_values[] = { 0, 2, 4, 6, 8, 15, 30, 60, 125, 250, 500, 1000, 2000 };
static const int shutter2_values[] = { 0, 1, 2, 4, 6, 8, 12, 15, 20, 25, 30, 40, 50, 60, 80, 100, 125, 160, 200, 250, 500, 1000, 2000 };

static void shooting_calc_autoiso_coef( int min_shutter )
{
    if (shutter2_values[conf.autoiso2_shutter_enum] >= min_shutter)
    {
        conf.autoiso2_coef = 0.0;
    }
    else
    {
        conf.autoiso2_coef = (float)(conf.autoiso2_max_iso_auto_real - conf.autoiso_max_iso_auto_real) / 
            (float)( shutter2_values[conf.autoiso2_shutter_enum] - min_shutter);
    }
}

static void shooting_recalc_conf_autoiso_values()
{
    // convert market to real iso
    conf.autoiso_max_iso_hi_real    = shooting_iso_market_to_real(conf.autoiso_max_iso_hi) ;
    conf.autoiso_max_iso_auto_real  = shooting_iso_market_to_real(conf.autoiso_max_iso_auto) ; 
    conf.autoiso_min_iso_real	    = shooting_iso_market_to_real(conf.autoiso_min_iso) ;      
    conf.autoiso2_max_iso_auto_real = shooting_iso_market_to_real(conf.autoiso2_max_iso_auto) ;

    // There are two exceptional situation: 
    // 1. shutter_numerator2 should be < shutter_numerator1, otherwise exceptional situation 
    // 2. autoiso2 <= autoiso1
    if ( !shutter2_values[conf.autoiso2_shutter_enum] )
        conf.autoiso2_max_iso_auto_real = conf.autoiso_max_iso_auto_real;

    // C2=( iso2_max_auto_real - iso_max_auto_real) / ( tv_num[autoiso2_shutter] - tv_numerator[autoiso_shutter])
    shooting_calc_autoiso_coef( shutter1_values[conf.autoiso_shutter_enum] );
}

void shooting_set_autoiso(int iso_mode)
{
    short max_iso;

    if (iso_mode<=0)
        shooting_recalc_conf_autoiso_values();

    switch (iso_mode)
    {
    case -1: // ISO HI
        //max_iso = conf.autoiso_max_iso_hi*10;
        max_iso = conf.autoiso_max_iso_hi_real;
        break;
    case 0: // ISO AUTO
        //max_iso = conf.autoiso_max_iso_auto*10;
        max_iso = conf.autoiso_max_iso_auto_real;
        break;
    default:
        return;
    }

    // TODO also long shutter ?
    if (camera_info.state.mode_shooting==MODE_M || camera_info.state.mode_shooting==MODE_TV || camera_info.state.mode_shooting==MODE_STITCH)
        return; //Only operate outside of M and Tv

    int ev_overexp = 0;
    if (conf.overexp_ev_enum)
    {
        unsigned short *histo = malloc(256*sizeof(unsigned short));
        if(histo) {
            // No shoot_histogram exist here because no future shot exist yet :)
            int total = live_histogram_read_y(histo);

            // step 32 is 1/3ev for tv96
            if (live_histogram_get_range(histo,total,255-conf.autoiso2_over,255) >= conf.overexp_threshold)
                ev_overexp = conf.overexp_ev_enum << 5; 
            free(histo);
        }
    }

    float current_shutter = shooting_get_shutter_speed_from_tv96(shooting_get_tv96());

    short current_iso = shooting_get_iso_real();

    short min_shutter = shutter1_values[conf.autoiso_shutter_enum];
    if (min_shutter == 0)
    {
        short IS_factor = (shooting_get_is_mode()<=1)?conf.autoiso_is_factor:1;
        min_shutter = get_focal_length(lens_get_zoom_point())*conf.autoiso_user_factor / (IS_factor*1000);
        //min_shutter is NOT 1/Xs but optimized for the calculation.
        if (shutter2_values[conf.autoiso2_shutter_enum])
            shooting_calc_autoiso_coef( min_shutter );
    }

    short target_iso = current_iso * min_shutter * current_shutter;
    short min_iso = conf.autoiso_min_iso_real;

    if (target_iso > max_iso)
    {
        ev_overexp=0;

        // AutoISO2 if
        // 	it is turned on (C2!=0.0)
        //	and it has valid iso2/shutter2 ( C2<0)
        //       and non-IsoHI mode
        if ( !iso_mode && conf.autoiso2_coef < 0.0 )
        {
            target_iso = (max_iso - min_shutter*conf.autoiso2_coef) / ( 1.0 - conf.autoiso2_coef  / (current_shutter * current_iso) );
            if ( target_iso > conf.autoiso2_max_iso_auto_real )
                target_iso = conf.autoiso2_max_iso_auto_real;
        }
        else
        {
            target_iso = max_iso;
        }
    }
    else if (target_iso < min_iso)
    {
        target_iso = min_iso;
    }

    float target_shutter = current_shutter *  current_iso / target_iso;

	if (target_shutter > 0)
        shooting_set_tv96_direct(shooting_get_tv96_from_shutter_speed(target_shutter) + ev_overexp, SET_NOW);

    shooting_set_iso_real(target_iso, SET_NOW);
}

//-------------------------------------------------------------------
