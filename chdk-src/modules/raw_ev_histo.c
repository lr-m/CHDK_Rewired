#include "camera_info.h"
#include "conf.h"
#include "raw_ev_histo.h"
#include "raw.h"
#include "file_counter.h"
#include "gui.h"
#include "gui_draw.h"
#include "math.h"
#include "dirent.h"

//-----------------------------------------------------------------------------

int running = 0;

#define EV_MAX14    104     // Max # of slots for 14 bit cameras.
#define BH          31      // Height of histogram area drawn on screen

typedef struct
{
    int ev_max;
    // 1/9 stop histogram buckets + over and under exposed count
    unsigned int histo[3][EV_MAX14];
    unsigned int over_exposed[3], under_exposed[3];
    // Scaled bar heights to fit drawn histogram
    unsigned char histo_bar[3][EV_MAX14];
    unsigned char under_bar[3], over_bar[3];
    unsigned short under_pct[3], over_pct[3];
    unsigned char ev_mark[EV_MAX14];
    unsigned short ev_mark_cnt;
    short ettr_hint[3];
    unsigned int sample_count, histo_max;
    int over_limit, under_limit;
} raw_histo;

raw_histo rh;

unsigned char* r2h = 0;
int last_black_level;
char last_playback_img[9];
char last_playback_file[65];
int histo_ready = 0;
int histo_drawn = 0;
int histo_drawn_cnt = 0;

//-----------------------------------------------------------------------------

// Find matching image file for histogram file
static int find_image(char* iname, char* fname)
{
    char fn[64];

    DIR *d;
    struct dirent *de;
    d = opendir("A/DCIM");
    int found = 0;
    if (d)
    {
        while ((de = readdir(d)) && !found)
        {
            if ((de->d_name[0] != '\xE5') && (strcmp(de->d_name,".") != 0) && (strcmp(de->d_name,"..") != 0) && (strcmp(de->d_name, "CANONMSC") != 0))
            {
                sprintf(fn, "A/DCIM/%s", de->d_name);
                struct stat st;
                stat(fn, &st);
                if (st.st_attrib & DOS_ATTR_DIRECTORY)
                {
                    // Image folder exists
                    DIR *id;
                    struct dirent *ide;
                    id = opendir(fn);
                    if (id)
                    {
                        while ((ide = readdir(id)) && !found)
                        {
                            if ((strlen(ide->d_name) == 12) && (strncmp(ide->d_name+4, iname+4, 4) == 0) && ((strncmp(ide->d_name+9, "JPG", 3) == 0) || (strncmp(ide->d_name+9, "CR2", 3) == 0)))
                            {
                                if (fname)
                                    sprintf(fname, "%s/%s", fn, ide->d_name);
                                found = 1;
                            }
                        }
                        closedir(id);
                    }
                }
            }
        }
        closedir(d);
    }

    return found;
}

//-----------------------------------------------------------------------------

// Delete orphaned histogram file - no matching image
static void delete_orphan(char* iname)
{
    char fn[64];
    sprintf(fn, "%s/IMG_%s.HST", "A/CHDK/RAWHST", iname+4);
    remove(fn);
}

// Delete all orphaned histogram files
static void delete_orphans()
{
    DIR *d;
    struct dirent *de;
    d = opendir("A/CHDK/RAWHST");
    if (d)
    {
        while ((de = readdir(d)))
        {
            if ((de->d_name[0] != '\xE5') && (strcmp(de->d_name,".") != 0) && (strcmp(de->d_name,"..") != 0))
            {
                char fn[64];
                sprintf(fn, "A/CHDK/RAWHST/%s", de->d_name);
                if (!find_image(de->d_name, 0))
                    remove(fn);
            }
        }
        closedir(d);
    }
}

//-----------------------------------------------------------------------------

// enable or disable shot histogram
static void load_raw_ev_histo(int enable)
{
    if (!running && enable)
    {
        delete_orphans();
        last_black_level = -1;
        last_playback_img[0] = 0;
        last_playback_file[0] = 0;
    }

    running = enable;
}

//-----------------------------------------------------------------------------

static void write_row(FILE *fd, char *c, int n)
{
    char fn[64];
    int i;

    sprintf(fn, "%s,%d.%0d,%d,", c, rh.under_pct[n]/10, rh.under_pct[n]%10, rh.under_exposed[n]);
    fwrite(fn, strlen(fn), 1, fd);
    for (i=rh.ev_max-1; i>=0; i--) {
        sprintf(fn, "%d,", rh.histo[n][i]);
        fwrite(fn, strlen(fn), 1, fd);
    }
    sprintf(fn, "%d,%d.%d\n", rh.over_exposed[n], rh.over_pct[n]/10, rh.over_pct[n]%10);
    fwrite(fn, strlen(fn), 1, fd);
}

static void raw_ev_write_to_log()
{
    char fn[64];

    FILE* fd = fopen("A/raw_ev_histo.csv", "a");
    if (fd)
    {
        sprintf(fn, "Image %04d\n", get_target_file_num());
        fwrite(fn, strlen(fn), 1, fd);
        write_row(fd, "R", 0);
        write_row(fd, "G", 1);
        write_row(fd, "B", 2);
        fclose(fd);
    }
}

static void raw_ev_write_for_image()
{
    char fn[64];

    if (stat("A/CHDK/RAWHST", 0) != 0)
        mkdir("A/CHDK/RAWHST");

    sprintf(fn, "%s/IMG_%04d.HST", "A/CHDK/RAWHST", get_target_file_num());

    FILE *fd = fopen(fn, "wb");
    if (fd)
    {
        fwrite(&rh, sizeof(rh), 1, fd);
        fclose(fd);
    }
}

// Check if image and saved histogram file exist and load histogram data if valid.
static int raw_ev_read_for_image(char* iname)
{
    char fn[64];

    strcpy(last_playback_img, iname);

    if (find_image(iname, last_playback_file))
    {
        // Found image - check for saved histogram
        sprintf(fn, "%s/IMG_%s.HST", "A/CHDK/RAWHST", iname+4);

        FILE *fd = fopen(fn, "rb");
        if (fd)
        {
            fread(&rh, sizeof(rh), 1, fd);
            fclose(fd);

            return 1;
        }
    }

    return 0;
}

//-----------------------------------------------------------------------------

// pow(2, n/9) for 0 <= n < 9
double pow2_inc[9] = {
    1.0,
    1.080059739,
    1.16652904,
    1.25992105,
    1.36079,
    1.469734492,
    1.587401052,
    1.714487966,
    1.851749425
};

static int r2h_ev(int start, int end, int* h)
{
    if (start < end)
    {
        memset(r2h+start+camera_sensor.black_level,*h,end-start);
        *h += 1;
    }
    return start;
}

// Build table mapping raw pixel values to histogram buckets
static void init_r2h()
{
    if (r2h == 0)
    {
        r2h = malloc(camera_sensor.white_level + 1);
    }

    if (r2h != 0)
    {
        int max = camera_sensor.white_level - camera_sensor.black_level + 1;
        double ev = log2(max);
        int h = 0;
        rh.ev_mark[0] = h;
        rh.ev_mark_cnt = 1;
        int s;
        do
        {
            ev = ev - 1.0;
            double d = pow(2, ev);
            s = (int)(d+0.5);
            if (s >= 1)
            {
                int i;
                for (i = 8; i > 0; i--)
                {
                    max = r2h_ev((int)(d*pow2_inc[i]+0.5), max, &h);
                }
                max = r2h_ev(s, max, &h);
            }
            rh.ev_mark[rh.ev_mark_cnt] = h;
            rh.ev_mark_cnt += 1;
        } while (s > 0);
        memset(r2h, h, camera_sensor.black_level+1);
        rh.ev_max = h + 1;
    }
}

static void add_histo(short v, int idx)
{
    if (v > rh.over_limit)
    {
        rh.over_exposed[idx]++;
        if (rh.over_exposed[idx] > rh.histo_max) rh.histo_max = rh.over_exposed[idx];
    }
    else if (v < rh.under_limit)
    {
        rh.under_exposed[idx]++;
        if (rh.under_exposed[idx] > rh.histo_max) rh.histo_max = rh.under_exposed[idx];
    }
    rh.histo[idx][r2h[v]]++;
    if (rh.histo[idx][r2h[v]] > rh.histo_max) rh.histo_max = rh.histo[idx][r2h[v]];
}

void build_raw_ev_histo()
{
    if (running)
    {
        int x, y, x0, x1, y0, y1;

        if ((r2h == 0) || (camera_sensor.black_level != last_black_level))
        {
            last_black_level = camera_sensor.black_level;   // Some cameras can change black level based on ISO and Tv
            init_r2h();
        }

        if (r2h == 0) return; // !! no memory

        memset(rh.histo, 0, 3 * EV_MAX14 * 4);
        memset(rh.over_exposed, 0, 3 * 4);
        memset(rh.under_exposed, 0, 3 * 4);
        rh.histo_max = 0;

        // Use sensor active area only
        int fw = camera_sensor.active_area.x2 - camera_sensor.active_area.x1;
        int fh = camera_sensor.active_area.y2 - camera_sensor.active_area.y1;

        int width = (fw * conf.raw_ev_histo_width) / 100;
        int height = (fh * conf.raw_ev_histo_height) / 100;

        // Histogram sample area
        x0 = (camera_sensor.active_area.x1 + (fw - width) / 2) & 0xFFFFFFFE;
        x1 = x0 + width;
        y0 = (camera_sensor.active_area.y1 + (fh - height) / 2) & 0xFFFFFFFE;
        y1 = y0 + height;

        rh.sample_count = ((x1 - x0 + conf.raw_ev_histo_xstep - 1) / conf.raw_ev_histo_xstep) * ((y1 - y0 + conf.raw_ev_histo_ystep - 1) / conf.raw_ev_histo_ystep);

        rh.under_limit = camera_sensor.black_level + conf.raw_ev_histo_under_threshold;
        rh.over_limit = camera_sensor.white_level - conf.raw_ev_histo_over_threshold;

        // Sample raw image and create histogram data
        for (y = y0; y < y1; y += conf.raw_ev_histo_ystep)
        {
            for (x = x0; x < x1; x += conf.raw_ev_histo_xstep)
            {
                short r = get_raw_pixel(x+camera_sensor.cfa_offsets[CFA_R][0],y+camera_sensor.cfa_offsets[CFA_R][1]);
                short g1 = get_raw_pixel(x+camera_sensor.cfa_offsets[CFA_G1][0],y+camera_sensor.cfa_offsets[CFA_G1][1]);
                short g2 = get_raw_pixel(x+camera_sensor.cfa_offsets[CFA_G2][0],y+camera_sensor.cfa_offsets[CFA_G2][1]);
                short b = get_raw_pixel(x+camera_sensor.cfa_offsets[CFA_B][0],y+camera_sensor.cfa_offsets[CFA_B][1]);

                add_histo(r, 0);
                add_histo((g1+g2)/2, 1);
                add_histo(b, 2);
            }
        }

        // Generate display values for histogram
        for (x = 0; x < 3; x++)
        {
            rh.under_pct[x] = (rh.under_exposed[x] * 1000) / rh.sample_count;
            rh.over_pct[x] = (rh.over_exposed[x] * 1000) / rh.sample_count;
            
            for (y = 0; y < rh.ev_max; y++)
            {
                rh.histo_bar[x][y] = (rh.histo[x][y] * BH) / rh.histo_max;
            }

            rh.under_bar[x] = (rh.under_exposed[x] * BH) / rh.histo_max;
            rh.over_bar[x] = (rh.over_exposed[x] * BH) / rh.histo_max;

            unsigned int ettr_limit = conf.raw_ev_histo_ettr_pct * rh.sample_count / 1000;
            if (rh.over_exposed[x] > ettr_limit)
            {
                rh.ettr_hint[x] = -1;
            }
            else
            {
                unsigned int sum = 0;
                for (y = 0; y < rh.ev_max; y++)
                {
                    sum += rh.histo[x][y];
                    if (sum >= ettr_limit)
                        break;
                }
                rh.ettr_hint[x] = y * 10 / 9;
            }
        }

        histo_ready = 1;

        if (conf.raw_ev_histo_save_log)
            raw_ev_write_to_log();

        if (conf.raw_ev_histo_save_for_image)
            raw_ev_write_for_image();
    }
}

//-----------------------------------------------------------------------------

// Erase from screen and reset values
static void erase_raw_ev_histo()
{
    if (histo_drawn)
    {
        histo_ready = 0;
        histo_drawn = 0;
        histo_drawn_cnt = 0;
        last_playback_img[0] = 0;
        last_playback_file[0] = 0;

        int hx = conf.raw_ev_histo_pos.x;
        int hy = conf.raw_ev_histo_pos.y;

        draw_rectangle(hx, hy, hx+rh.ev_max+7+FONT_WIDTH*8, hy+3*BH+3, MAKE_COLOR(COLOR_TRANSPARENT,COLOR_TRANSPARENT), RECT_BORDER0|DRAW_FILLED);
        gui_set_need_restore();
    }
}

static void draw_over_under_pct(int x, int y, int pct, int i)
{
    if (pct > 0)
    {
        char s[10];
        if (pct >= 100)
            sprintf(s, "%d%%", pct/10);
        else
            sprintf(s, "%d.%d%%", pct/10, pct%10);
        draw_string(x, y+i*BH+i+BH-FONT_HEIGHT, s, MAKE_COLOR(COLOR_BLACK,COLOR_WHITE));
    }
}

static void draw_bar(int x, int y, int val, int i, color col)
{
    if (val > 0)
        draw_vline(x, y+BH+i*BH+i-val+1, val, col);
}

static void draw_raw_ev_histo(int force_redraw)
{
    if (running)
    {
        // Load saved histogram in playback mode
        if (camera_info.state.mode_play && conf.raw_ev_histo_save_for_image)
        {
            char* n = get_playback_image_number();
            if (n)
            {
                // Displayed image changed?
                if (strcmp(n, last_playback_img) != 0)
                {
                    if (raw_ev_read_for_image(n))
                    {
                        histo_ready = 1;
                        histo_drawn = 0;
                        histo_drawn_cnt = 0;
                    }
                    else
                    {
                        erase_raw_ev_histo();
                    }
                }
                else if (histo_drawn_cnt >= 5)
                {
                    // Already loaded and drawn?
                    if (histo_drawn_cnt >= 10)
                    {
                        histo_drawn_cnt = 5;
                        if (stat(last_playback_file, 0) != 0)
                        {
                            // Image file deleted
                            delete_orphan(last_playback_img);
                            erase_raw_ev_histo();
                            return;
                        }
                    }
                    if (!force_redraw)
                    {
                        histo_drawn_cnt++;
                        return;
                    }
                }
            }
        }
        else
        {
            last_playback_img[0] = 0;
        }

        if (histo_ready)
        {
            int i, j;

            color rgb[3] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE};

            int hx = conf.raw_ev_histo_pos.x + 4*FONT_WIDTH;
            int hy = conf.raw_ev_histo_pos.y;

            draw_rectangle(hx-4*FONT_WIDTH, hy, hx+rh.ev_max+7+4*FONT_WIDTH, hy+3*BH+3, MAKE_COLOR(COLOR_BLACK,COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_rectangle(hx, hy, hx+rh.ev_max+7, hy+3*BH+3, MAKE_COLOR(COLOR_TRANSPARENT,COLOR_GREY_DK),RECT_BORDER1);
            for (i = 0; i < rh.ev_mark_cnt; i += 1)
                draw_vline(hx+4+(rh.ev_max-1-rh.ev_mark[i]), hy+1, 3*BH+2, COLOR_GREY_DK);
            draw_hline(hx+1, hy+BH+1, rh.ev_max+6, COLOR_GREY_DK);
            draw_hline(hx+1, hy+2*BH+2, rh.ev_max+6, COLOR_GREY_DK);

            for (i=0; i<3; i++)
            {
                for (j = 0; j < rh.ev_max; j++)
                    draw_bar(hx+4+(rh.ev_max-1-j), hy, rh.histo_bar[i][j], i, rgb[i]);

                draw_bar(hx+2, hy, rh.under_bar[i], i, COLOR_WHITE);
                draw_bar(hx+5+rh.ev_max, hy, rh.over_bar[i], i, COLOR_WHITE);

                draw_over_under_pct(conf.raw_ev_histo_pos.x, hy, rh.under_pct[i], i);
                draw_over_under_pct(hx+8+rh.ev_max, hy, rh.over_pct[i], i);

                int hint = rh.ettr_hint[i];
                if (hint > 0)
                {
                    char s[10];
                    if (hint >= 100)
                        strcpy(s, ">10");
                    else
                        sprintf(s, ">%d.%d", hint / 10, hint % 10);
                    draw_string(hx+8+rh.ev_max, hy+i*BH+i, s, MAKE_COLOR(COLOR_BLACK,COLOR_WHITE));
                }
            }

            histo_drawn = 1;
            histo_drawn_cnt++;
        }
    }
}

//-----------------------------------------------------------------------------

int _module_loader()
{
    return 0;
}

int _module_unloader()
{
    if (r2h)
    {
        free(r2h);
        r2h = 0;
    }
    return 0;
}

int _module_can_unload()
{
    return !running && !histo_drawn;
}

// =========  MODULE INIT =================

librawevhisto_sym _librawevhisto =
{
    {
         _module_loader, _module_unloader, _module_can_unload, 0, 0
    },

    load_raw_ev_histo,
    build_raw_ev_histo,
    draw_raw_ev_histo,
    erase_raw_ev_histo,
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    RAW_EV_HISTO_VERSION,       // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,        // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    (int32_t)"RAW EV Histogram",
    MTYPE_EXTENSION,

    &_librawevhisto.base,

    ANY_VERSION,                // CONF version
    ANY_VERSION,                // CAM SCREEN version
    CAM_SENSOR_VERSION,         // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};

//-----------------------------------------------------------------------------
