#include "camera_info.h"
#include "conf.h"
#include "shot_histogram.h"
#include "raw.h"
#include "file_counter.h"

#define SHOT_HISTOGRAM_STEP     31

// we could use sensor bitdepth here. For now, we just discard low bits if > 10bpp
#define SHOT_HISTOGRAM_SAMPLES  1024
#define SHOT_HISTOGRAM_SIZE     (SHOT_HISTOGRAM_SAMPLES*sizeof(short))

int running = 0;

// Buffer to sum raw luminance values
unsigned short shot_histogram[SHOT_HISTOGRAM_SAMPLES];

// Sensor area to process (not used)
unsigned short shot_margin_left = 0, shot_margin_top = 0, shot_margin_right = 0, shot_margin_bottom = 0;

// enable or disable shot histogram
int shot_histogram_set(int enable)
{
    // If turning shot histogram off, mark module as unloadable
    if (!enable)
        running = 0;

    return 1;
}

void build_shot_histogram()
{
    // read samples from RAW memory and build an histogram of its luminosity
    // actually, it' just reading pixels, ignoring difference between R, G and B,
    // we just need an estimate of luminance
    if (!running)
        return;

    int x, y, x0, x1, y0, y1;

    short p;
    memset(shot_histogram,0,SHOT_HISTOGRAM_SIZE);

    // Use sensor active area only
    int width  = camera_sensor.active_area.x2 - camera_sensor.active_area.x1;
    int height = camera_sensor.active_area.y2 - camera_sensor.active_area.y1;

    // In future, support definition of a sort of "spot metering"
    x0 = camera_sensor.active_area.x1 + ((shot_margin_left   * width)  / 10);
    x1 = camera_sensor.active_area.x2 - ((shot_margin_right  * width)  / 10);
    y0 = camera_sensor.active_area.y1 + ((shot_margin_top    * height) / 10);
    y1 = camera_sensor.active_area.y2 - ((shot_margin_bottom * height) / 10);

    // just read one pixel out of SHOT_HISTOGRAM_STEP, one line out of SHOT_HISTOGRAM_STEP
    if (camera_sensor.bits_per_pixel == 10)
    {
        for (y = y0; y < y1; y += SHOT_HISTOGRAM_STEP)
            for (x = x0; x < x1; x += SHOT_HISTOGRAM_STEP)
            {
                p = get_raw_pixel(x,y);
                shot_histogram[p]++;
            }
    }
    else
    {
        // > 10bpp compatibility: discard the lowest N bits
        int shift = (camera_sensor.bits_per_pixel - 10);
        for (y = y0; y < y1; y += SHOT_HISTOGRAM_STEP )
            for (x = x0; x < x1; x += SHOT_HISTOGRAM_STEP )
            {
                p = get_raw_pixel(x,y) >> shift;
                shot_histogram[p]++;
            }
    }
}

void write_to_file()
{
    // dump last histogram to file
    // for each shoot, creates a HST_nnnn.DAT file containing 2*1024 bytes

    char fn[64];

    // Get path to save raw files
    raw_get_path(fn);

    sprintf(fn+strlen(fn), "HST_%04d.DAT", get_target_file_num());

    int fd = open(fn, O_WRONLY|O_CREAT, 0777);
    if (fd >= 0)
    {
        write(fd, shot_histogram, SHOT_HISTOGRAM_SIZE);
        close(fd);
    }
}

int shot_histogram_get_range(int histo_from, int histo_to)
{
    // Examines the histogram, and returns the percentage of pixels that
    // have luminance between histo_from and histo_to
    int x, tot, rng;
    tot=0;
    rng=0;
 
    if (!running)
        return -1;

    for (x = 0 ; x < SHOT_HISTOGRAM_SAMPLES; x++)
    {
        tot += shot_histogram[x];
        if (x>=histo_from && x <= histo_to)
        {
            rng += shot_histogram[x];
        }
    }

    return (rng*100+tot/2)/tot;
}

//-----------------------------------------------------------------------------
int _module_loader()
{
    running = 1;
    return 0;
}

int _module_unloader()
{
    return 0;
}

int _module_can_unload()
{
    return running == 0;
}

// =========  MODULE INIT =================

libshothisto_sym _libshothisto =
{
    {
         _module_loader, _module_unloader, _module_can_unload, 0, 0
    },

    shot_histogram_set,
    shot_histogram_get_range,
    build_shot_histogram,
    write_to_file,
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    SHOT_HISTO_VERSION,         // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,        // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    (int32_t)"Shot Histogram",
    MTYPE_EXTENSION,

    &_libshothisto.base,

    ANY_VERSION,                // CONF version
    ANY_VERSION,                // CAM SCREEN version
    CAM_SENSOR_VERSION,         // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};
