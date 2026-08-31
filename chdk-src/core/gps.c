
#include "camera_info.h"
#include "properties.h"
#include "task.h"
#include "modes.h"
#include "debug_led.h"
#include "shutdown.h"
#include "sound.h"
#include "backlight.h"
#include "temperature.h"
#include "file_counter.h"
#include "gps.h"
#include "gps_math.h"
#include "time.h"

#include "conf.h"
#include "keyboard.h"
#include "lang.h"
#include "levent.h"
#include "math.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_batt.h"
#include "gui_lang.h"
#include "gui_mbox.h"

// For testing on cameras without a GPS chip - uncomment the next line
//    #define SIMULATED_GPS   1
// for testing on cameras without a GPS chip - add the following lines (uncommented) to the relevant platform_camera.h file
/*
    #define CAM_HAS_GPS     1
    #define PROPCASE_GPS    0       // Add this if the cameras propset does not include it
*/

// gps task control

static int gps_data_lock ;      // task running flags
static int gps_show_lock;
static int gps_record_lock;
static int gps_track_lock;
static int gps_compass_lock ;
static int gps_nosignal_lock;    

static int exit_data_task =0;  // request task to exit flags
static int exit_show_task=0;
static int exit_record_task=0;
static int exit_track_task=0;
static int exit_compass_task=0;
static int exit_nosignal_task=0;

//static data for gps task

static int test_timezone_once=0;

static char g_d_tim[10];
static char g_d_dat[12];
static char g_d_tim_gps[10];
static char g_d_dat_gps[12];
static int  g_d_stat;

static double g_d_lat_nav=0.0;
static double g_d_lon_nav=0.0;
static double g_d_lat;
static double g_d_lon;
static double g_d_hei;

static int time_to_end=0;

static int navigation_mode=0;       // 0=none, 1=to image,  2=home

static t_regression_xy buffer1[50];
static t_regression_xy buffer2[50];
static t_regression_xy buffer3[50];

#define PI      (3.1415926535)
#define RHO     (180.0/3.1415926535)
#define EARTH   (40.e6/360.0)

typedef struct {
    double lat_w;
    double lon_w;
} t_geo;

typedef struct {
    double north;
    double east;
} t_rectangular;

typedef struct {
    double delta;
    double alpha;   // radian
} t_polar;

static void
geodiff (t_geo * von, t_geo* nach, t_geo* delta) {
    delta->lat_w = nach->lat_w - von->lat_w;
    delta->lon_w = nach->lon_w - von->lon_w;
}

static void
geo2rect (t_geo * g, t_rectangular* r, double lat_w) {
    r->north= g->lat_w * EARTH;
    r->east = g->lon_w * EARTH * cos(lat_w/RHO);
}

static void
rect2polar (t_rectangular* r, t_polar* p) {
    p->delta = sqrt (r->north*r->north + r->east*r->east);
    p->alpha = arctan2 (r->east, r->north);
}

#if 0
static void
polar2rect (t_rectangular* r, t_polar* p) {
    r->east = sin(p->alpha) * p->delta;
    r->north= cos(p->alpha) * p->delta;
}
#endif

static int
radian2deg (double alpha) {
    return (int) (alpha * RHO + 360.5) % 360;
}

typedef struct {
    t_regression lat_w;
    t_regression lon_w;
} t_georeg;

static void
georegInit (t_georeg* reg, int size, t_regression_xy * buffer1, t_regression_xy * buffer2) {
    regressionInit (&reg->lat_w, size, buffer1);
    regressionInit (&reg->lon_w, size, buffer2);
}

static void
georegAdd (t_georeg* reg, double time, t_geo* geo) {
    regressionAdd (&reg->lat_w, time, geo->lat_w);
    regressionAdd (&reg->lon_w, time, geo->lon_w);
}

static void
georegActual (t_georeg* reg, t_geo* geo) {
    geo->lat_w = regressionActual (&reg->lat_w);
    geo->lon_w = regressionActual (&reg->lon_w);
}

static void
georegChange (t_georeg* reg, t_geo* geo) {
    geo->lat_w = regressionChange (&reg->lat_w);
    geo->lon_w = regressionChange (&reg->lon_w);
}

static double now;                                     // seconds of day
static t_geo gps;                                      // measured noisy position
static t_georeg georeg;                                // regression for actual position
static t_geo ist;                                      // actual position of regression
static t_geo speed;                                    // actual position of regression in Grad/s
static t_rectangular mspeed;                           // actual position of regressionin m/s (N/E)
static t_polar pspeed;                                 // actual position of regression in m/s + angle
static int direction;                                  // direction
static t_geo soll;                                     // nominal position
static t_geo delta;                                    // distance to destination(in Grad)
static t_rectangular rdelta;                           // distance to destination (metric N/E)
static t_polar pdelta;                                 // distance to destination (metric+english)
static int peil;                                       // bearing to destination
static int angle;                                      // angle on display from march to the target
static t_regression deltareg;                          // heading and speed

#ifdef SIMULATED_GPS
    #include "gps_simulate.c"  
#endif

//=======================================================================
//
//  Bakery algoritmn mutex lock / unlock
//
#define NUM_TASKS 6
static volatile unsigned entering[NUM_TASKS] = { 0 };
static volatile unsigned number[NUM_TASKS] = { 0 };

void lock(int n_task) {
  int i ;
  unsigned max = 0;    
  entering[n_task] = 1;
  for (i = 0; i < NUM_TASKS; i++) {
    if (number[i] > max) max = number[i];
  }
  number[n_task] = max + 1 ;
  entering[n_task] = 0;
  for (i = 0; i < NUM_TASKS; i++) {
    if( i != n_task) {
        while ( entering[i] !=0 ) { msleep(50) ; }
        while ((number[i] != 0) && ( number[i] < number[n_task] || (number[i] == number[n_task] && i < n_task) )) { msleep(50);}
    }
  }
}
 
void unlock(int n_task)
{
  number[n_task] = 0;
}
 
//=======================================================================
//
//  Helper functions - called from within running tasks
//

void draw_txt_centered(int line, char *buf, int color)
{
    draw_txt_string( (camera_screen.width/FONT_WIDTH-strlen(buf))>>1 , line, buf, color) ;
}

static void compass_display_task();
static void run_compass_task(int mode){  
    
    navigation_mode = mode ;
    
    if (gps_compass_lock == 0 )
    {
        gps_compass_lock = 1 ;   
        CreateTask("GPS_COMPASS", 0x19, 0x1000, compass_display_task);   
    }
}

static void test_timezone(){                           // creates timezone.txt file with current timezone info
  
    char text[2][30];
    char home_name[30];
    int timezone_1, timezone_2;
    char * endptr;

    sprintf(home_name, "A/GPS/Timezone/Timezone.txt");

    FILE* fp = fopen(home_name, "r");
    if( fp )
    {
        fgets(text[1], 15, fp);
        fgets(text[2], 15, fp);
        fclose(fp);

        g_d_lat_nav = (strtol (text[1], &endptr, 10 )) / 10000000.0;
        g_d_lon_nav = (strtol (text[2], &endptr, 10 )) / 10000000.0;

        timezone_1 = (int)((g_d_lon_nav+7.5)/15);
        timezone_2 = (int)((g_d_lon+7.5)/15);

        if (timezone_1 != timezone_2)
        {
            gui_mbox_init(LANG_INFORMATION, LANG_MENU_GPS_t_2, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL); //Timezone has changed!
        }
    }
}

static char *load_bitmap(char *datei){
    
    FILE *fd;
    char *bitmap;
    int bitmap_size;

    fd = fopen ( datei, "r" );

    fseek (fd, 0, SEEK_END);
    bitmap_size = ftell (fd);
    fseek (fd, 0, SEEK_SET);

    bitmap = (char*) malloc (sizeof(char)*bitmap_size);
    fread (bitmap,1,bitmap_size,fd);
    fclose (fd);

    return bitmap;
}

static double draw_gps_course_info(int count){             // calculate & display course information

    char vBuf[512];
    int angle1=0;

    soll.lat_w = g_d_lat_nav;
    soll.lon_w = g_d_lon_nav;

    now = (double) (count);
    gps.lat_w = g_d_lat;                                                    // read GPS (with noise)
    gps.lon_w = g_d_lon;
    georegAdd               (&georeg, now, &gps);                           // feed the regression
    georegActual            (&georeg, &ist);                                // read averaged position
    georegChange            (&georeg, &speed);                              // read averaged velocity
    geo2rect                (&speed, &mspeed, ist.lat_w);                   // Convert to m/s
    rect2polar              (&mspeed, &pspeed);                             // convert to direction and speed.
    direction = radian2deg  (pspeed.alpha);                                 // get direction
    geodiff                 (&ist, &soll, &delta);                          // Distance in degrees (N/E)
    geo2rect                (&delta, &rdelta, ist.lat_w);                   // Distance in m (je N/E)
    rect2polar              (&rdelta, &pdelta);                             // Distance in degrees m and grad
    peil    = radian2deg    (pdelta.alpha);                                 // Bearing to destination
    angle  = radian2deg     (pdelta.alpha-pspeed.alpha);                    // Angle relative to the direction of march
    regressionAdd           (&deltareg, now, pdelta.delta);                 // do the regression
    double eta = regressionReverse (&deltareg, 0);                          // estimated time of arrival
    double rest= eta - now;                                                 // Time = arrival time - now

    // TODO: check this???
    // This may be wrong as the CHDK 'abs' function operates on an int parameter and returns an int value
    // Conversion of the regressionChange value to an int means the test will be true for any value from -1.0 to 1.0, not -0.5 to 0.5 as the code suggests.
    if (abs(regressionChange (&deltareg))<0.5 || rest < 5.0) rest = 0.0;

    if (camera_info.state.gui_mode_none)
    {
        angle1=(int)angle;
        char anz1[40];
        char anz2[40];
        char image[9];        

        if (navigation_mode > 0)
        {
            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_9), (int)pdelta.delta);
            draw_txt_string(16, 9, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_WHITE));

            int s = (int)rest;
            int hour = s / 3600;
            s = s % 3600;
            int minute =  s / 60;
            s = s % 60;
            int second = s;

            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_10), hour, minute, second);
            draw_txt_string(16, 10, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_WHITE));

            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_11), formatDouble (anz1, (pspeed.delta * 3.6), 0, 1));
            draw_txt_string(16, 11, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_WHITE));
            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_12), direction);
            draw_txt_string(16, 12, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_WHITE));
            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_13), (int)angle);
            draw_txt_string(16, 13, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_WHITE));

            if (navigation_mode==1)
            {
                sprintf(image, "%s", camera_jpeg_current_filename());
                image[8] = '\0';
                sprintf(vBuf, lang_str(LANG_MENU_GPS_t_14), image);  // "Navigation to photo: %s started"
                draw_txt_centered(1, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_RED));
            }
            
            if (navigation_mode==2)
            {
                sprintf(vBuf, lang_str(LANG_MENU_GPS_t_17));         // "Navigation to Home started"
                draw_txt_centered(1, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_RED));
            }

            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_15), formatDouble (anz1, g_d_lat_nav, 0, 7), formatDouble (anz2, g_d_lon_nav, 0, 7)); //"latitude=%s  -  longitude=%s "
            draw_txt_centered(2, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_RED));
        }
        else
        {
            angle1= direction;
            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_16), (int)angle1);   // "heading = %i°"
            draw_txt_string(16, 10, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_WHITE));            
            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_15), formatDouble (anz1, g_d_lat_nav, 0, 7), formatDouble (anz2, g_d_lon_nav, 0, 7)); //"latitude=%s  -  longitude=%s "
            draw_txt_string(16, 11, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_WHITE));  
        }
    }

    return angle1;
}

// draw animated compass needle on GUI
static void draw_compass_needle (int angle, double s_w, double c_w, char *bitmap, int extent, int m_x, int m_y, int offset_x, int offset_y, int f_v_1, int f_v_2){
 
/*
    char vBuf[32];
    sprintf(vBuf, "%d", angle );      
    draw_txt_string(30, 0, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_RED));   
*/
    if(bitmap)
    {
        int mx=0;
        int my=0;
        int px=0;
        int py=0;

        int pos=0;
        int x_n;
        int y_n;

        if (angle==0 || angle==360)
        {
            s_w = 0;
            c_w = -1;
        }

        if (angle==270)
        {
            s_w = -1;
            c_w = 0;
        }

        for(pos=0; pos<extent; pos++)
        {
            int data = bitmap[pos];
            if (data >= 49)
            {
                px=offset_x+mx;
                py=offset_y+my;

                x_n = m_x + Round(((c_w * (m_x - px)) + (s_w * (m_y - py))),2);
                y_n = m_y + Round(((c_w * (m_y - py)) - (s_w * (m_x - px))),2);

                if (data == 49)
                {
                    draw_pixel(x_n, y_n, f_v_1);
                }
                if (data == 50)
                {
                    draw_pixel(x_n, y_n, f_v_2);
                }
            }
            if (data == 10 || data == 13)
            {
                mx=0;
                my++;
                pos++;
            }
            else
            {
                mx++;
            }
        }

    }
}

// draw animated compass on GUI
static void draw_compass (char *bitmap, int o_x, int o_y, int f_v_0, int f_v_1, int f_v_2, int f_v_4){

    if(bitmap)
    {
        int extent = strlen(bitmap);
        int pos=0;
        int mx=0;
        int my=0;

        for(pos=0; pos<extent; pos++)
        {
            int data = bitmap[pos];
            if (data == 48)
            {
                draw_pixel(o_x+mx, o_y+my, f_v_0);
            }
            if (data == 49)
            {
                draw_pixel(o_x+mx, o_y+my, f_v_1);
            }
            if (data == 50)
            {
                draw_pixel(o_x+mx, o_y+my, f_v_2);
            }
            if (data == 51)
            {
                draw_pixel(o_x+mx, o_y+my, f_v_4);
            }
            if (data == 10 || data == 13)
            {
                mx=0;
                my++;
                pos++;
            }
            else
            {
                mx++;
            }
        }

    }
}

//=======================================================================
//
// TASK : displays satellite dish image when CHDK GPS functionality enabled
//
static void show_sat_symbol_task(){
    
    char *bitmap = load_bitmap("A/CHDK/DATA/GPS_Sat.txt");

    if(bitmap)
    {
        int extent1 = strlen(bitmap);
        int pos1=0;
        int mx1=0;
        int my1=0;
        int o_x1=270;
        int o_y1=0;
        int f_v_0;
//         int f_h_0;
        int f_v_1;
//         int f_h_1;
        int f_v_2;
//         int f_h_2;
        int f_v_3;
//         int f_h_3;
        int f_v_4;
//         int f_h_4;

        while (exit_show_task==0) 
        {           
            int stat = g_d_stat;
            
            mx1=0;
            my1=0;

            f_v_0=COLOR_TRANSPARENT;
//             f_h_0=COLOR_TRANSPARENT;

            f_v_1=COLOR_GREEN;
//             f_h_1=COLOR_GREEN;

            switch (stat)
            {
                case 0 :
                    f_v_1= COLOR_RED ;
//                     f_h_1= COLOR_RED ;
                    break ;
                case 1 :
                    f_v_1= COLOR_YELLOW ;
//                     f_h_1= COLOR_YELLOW ;
                    break ;
                case 2 :
                    f_v_1= COLOR_GREEN ;
//                     f_h_1= COLOR_GREEN ;
                    break ;
            }
            
            if (camera_info.state.mode_play)
            {
                gps_img_data *igps = ( gps_img_data *) camera_jpeg_current_gps();

                f_v_1= COLOR_RED ;
//                 f_h_1= COLOR_RED ;
                if (igps->latitudeRef[0] && igps->longitudeRef[0])
                {
                    f_v_1= COLOR_YELLOW ;
//                     f_h_1= COLOR_YELLOW ;
                    if (igps->height[1])
                    {
                        f_v_1= COLOR_GREEN ;
//                         f_h_1= COLOR_GREEN ;
                    }
                }
            }

            f_v_2= COLOR_BLACK ;
//             f_h_2= COLOR_BLACK ;

            f_v_3= COLOR_YELLOW ;
//             f_h_3= COLOR_YELLOW ;

            f_v_4= COLOR_BLUE ;
//             f_h_4= COLOR_BLUE ;

            for(pos1=0; pos1<extent1; pos1++)
            {
                int data = bitmap[pos1];
                if (data == 48)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_0);
                }
                if (data == 49)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_1);
                }
                if (data == 50)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_2);
                }
                if (data == 51)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_3);
                }
                if (data == 52)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_4);
                }
                if (data == 10 || data == 13)
                {
                    mx1=0;
                    my1++;
                    pos1++;
                }
                else
                {
                    mx1++;
                }
            }
            msleep(1000);           
        }
        free(bitmap);
    }
    gps_show_lock = 0 ;     
    ExitTask();
}

//=======================================================================
//
// TASK : updates GPS data structures continuously if CHDK GPS functionality is enabled
//
static void gps_data_task(){
    
    test_timezone_once=0;
    
    while (exit_data_task==0)
    {
        tGPS gps;
        int tim00=0;
        int tim01=0;
        int tim02=0;
        unsigned long t;
        static struct tm *ttm;        
                          
  #ifndef SIMULATED_GPS
        GPS_UpdateData();  
        get_property_case(camera_info.props.gps, &gps, sizeof(gps));
  #else     
        GPS_FakeData() ;
        memcpy( &gps , &gps_dummy_data, sizeof(gps) );
  #endif
  
        t=time(NULL);
        ttm = localtime(&t);
        
        lock(0);
        
            g_d_lat=0.0;
            g_d_lat=(gps.latitude[0]/(gps.latitude[1]*1.0)) + (gps.latitude[2]/(gps.latitude[3]*60.0)) + (gps.latitude[4]/(gps.latitude[5]*3600.0));
            if (gps.latitudeRef[0]=='S') g_d_lat=-g_d_lat;

            g_d_lon=0.0;
            g_d_lon=(gps.longitude[0]/(gps.longitude[1]*1.0)) + (gps.longitude[2]/(gps.longitude[3]*60.0)) + (gps.longitude[4]/(gps.longitude[5]*3600.0));
            if (gps.longitudeRef[0]=='W') g_d_lon=-g_d_lon;

            g_d_hei=0.0;
            g_d_hei=gps.height[0]/(gps.height[1]*1.0);

    /**************
    char vBuf[256], lat[40],lon[40] ;
    sprintf(vBuf, "%d.%d.%d  %d.%d.%d",
        gps.latitude[0] / gps.latitude[1] , 
        gps.latitude[2] / gps.latitude[3] ,
        gps.latitude[4] / gps.latitude[5] ,   
        gps.longitude[0] / gps.longitude[1] , 
        gps.longitude[2] / gps.longitude[3] ,
        gps.longitude[4] / gps.longitude[5] );      
    draw_txt_string(1, 0, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_RED));    
    formatDouble (lat, g_d_lat, 0, 7),
    formatDouble (lon, g_d_lon, 0, 7),    
    sprintf(vBuf,"lat=%s lon=%s",lat,lon );
    draw_txt_string(1, 1, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_RED));              
    ************/
    
            sprintf(g_d_tim, "%02d:%02d:%02d", ttm->tm_hour, ttm->tm_min, ttm->tm_sec);
            sprintf(g_d_dat, "%04d-%02d-%02d", ttm->tm_year+1900, ttm->tm_mon+1, ttm->tm_mday);

            g_d_stat=0;
            if ((int)g_d_lat == 0) {g_d_stat = 0; }
            if ((int)g_d_lat != 0 && (int)g_d_hei == 0) {g_d_stat=1; }
            if ((int)g_d_lat != 0 && (int)g_d_hei != 0) {g_d_stat=2; }

            if (g_d_stat > 0)
            {
                tim00=(int)(gps.timeStamp[0]/gps.timeStamp[1]);
                tim01=(int)(gps.timeStamp[2]/gps.timeStamp[3]);
                tim02=(int)(gps.timeStamp[4]/gps.timeStamp[5]);
                sprintf(g_d_tim_gps, "%02d:%02d:%02d", tim00, tim01, tim02);

                sprintf(g_d_dat_gps, "%c%c%c%c-%c%c-%c%c",
                        gps.dateStamp[0],
                        gps.dateStamp[1],
                        gps.dateStamp[2],
                        gps.dateStamp[3],
                        gps.dateStamp[5],
                        gps.dateStamp[6],
                        gps.dateStamp[8],
                        gps.dateStamp[9]);

                if (((int)conf.gps_test_timezone == 1) && (test_timezone_once==0))
                {
                    test_timezone();
                    test_timezone_once=1;
                }
            }

        unlock(0);

        if ((int)conf.gps_show_symbol == 1)
        {
            if ( gps_show_lock == 0 )
            {                
                exit_show_task= 0 ;      
                gps_show_lock = 1 ;
                CreateTask("GPS_SHOW", 0x19, 0x800, show_sat_symbol_task);
            }
        } 
        else exit_show_task=1; 
        
        msleep(900);
    } 
    exit_show_task=1; 
    gps_data_lock = 0 ;    
    ExitTask();
}

//=======================================================================
//
// TASK : updates onsceen tracking icon (globe)
//
static void tracking_icon_task(){
    
    char * bitmap1 = load_bitmap("A/CHDK/DATA/GPS_Track_1.txt");
    char * bitmap2 = load_bitmap("A/CHDK/DATA/GPS_Track_2.txt");
    char * bitmap3 = load_bitmap("A/CHDK/DATA/GPS_Track_3.txt");

    if(bitmap1 && bitmap2 && bitmap2 )
    {
        int extent1 = strlen(bitmap1);
        int pos1=0;
        int mx1=0;
        int my1=0;
        int o_x1=315;
        int o_y1=0;
        int f_v_0;
//         int f_h_0;
        int f_v_1;
//         int f_h_1;
        int f_v_2;
//         int f_h_2;
        int f_v_3;
//         int f_h_3;
        int f_v_4;
//         int f_h_4;
        int f_v_5;
//         int f_h_5;
        int blink=0;
        int data;

        while (exit_track_task == 0)
        {
            mx1=0;
            my1=0;

            f_v_0=COLOR_TRANSPARENT;
//             f_h_0=COLOR_TRANSPARENT;

            f_v_1= COLOR_BLACK ;
//             f_h_1= COLOR_BLACK ;

            f_v_2= COLOR_BLUE ;
//             f_h_2= COLOR_BLUE ;

            f_v_3= COLOR_YELLOW ;
//             f_h_3= COLOR_YELLOW ;

            f_v_4= COLOR_RED ;
//             f_h_4= COLOR_RED ;

            f_v_5= COLOR_GREEN ;
//             f_h_5= COLOR_GREEN ;

            int stat = g_d_stat ;
            
            for(pos1=0; pos1<extent1; pos1++)
            {
                if ((blink==1) && (stat > 0))
                {
                    data = bitmap2[pos1];
                }
                else
                {
                    data = bitmap1[pos1];
                }
                if ((blink==1) && (navigation_mode > 0) && (stat > 0))
                {
                    data = bitmap3[pos1];
                }
                if (data == 48)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_0);
                }
                if (data == 49)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_1);
                }
                if (data == 50)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_2);
                }
                if (data == 51)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_3);
                }
                if (data == 52)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_4);
                }
                if (data == 53)
                {
                    draw_pixel(o_x1+mx1, o_y1+my1, f_v_5);
                }
                if (data == 10 || data == 13)
                {
                    mx1=0;
                    my1++;
                    pos1++;
                }
                else
                {
                    mx1++;
                }
            }

            msleep(1000);
            
            if (blink==0)
            {
                blink=1;
            }
            else
            {
                blink=0;
            }
        }        
    }
    if (bitmap1) free(bitmap1) ;
    if (bitmap2) free(bitmap2) ;
    if (bitmap3) free(bitmap3) ;
    gps_track_lock = 0 ; 
    ExitTask();
}

//=======================================================================
//
// TASK : started by waypoint recording code when no GPS signal and picture was taken
//
static void no_signal_task(){
   
    char vBuf[100];
    FILE *fd0;
    char w1[20];
    char w2[30];
    char w3[10];
    int o=1, p=1, q=0;
    int blite_off=0;
    unsigned long bat;
    int abort=0;
    
    gps_key_trap = KEY_DISPLAY ;
    
    while ( time_to_end !=0 )
    {

        // Test battery
        if ( (((time_to_end) % 60)) == 0 )
        {
            bat=get_batt_perc();
            if (bat <= (unsigned)conf.gps_batt)
            {
                int zba;
                for(zba=30; zba>0; zba--)
                {
                    sprintf(vBuf, lang_str(LANG_MENU_GPS_t_3));      // "Battery below setting!"
                    draw_txt_centered(8, vBuf, MAKE_COLOR(COLOR_YELLOW, COLOR_RED));
                    sprintf(vBuf, lang_str(LANG_MENU_GPS_t_4),zba); // "Camera will shutdown in %02d seconds!"
                    draw_txt_centered(9, vBuf, MAKE_COLOR(COLOR_RED, COLOR_BLUE));
                    if ( (((zba) % 2)) == 0 )
                    {
                        debug_led(0);
                        if ((int)conf.gps_batt_warn == 1)
                        {
                            play_sound(6);
                        }
                    }
                    else
                    {
                        debug_led(1);
                    }
                    msleep(1000);
                }
                camera_shutdown_in_a_second();
                gps_nosignal_lock = 0 ;                 
                ExitTask();
            }
        }

        // cancel automatic shut-off if DISP key pressed
        if (gps_key_trap == -1 )
        {
            gps_key_trap = 0;             
            abort = 1;
            time_to_end = 3600;
            TurnOnBackLight();
            gui_mbox_init(LANG_INFORMATION, LANG_MENU_GPS_t_5, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL); //Automatic shutdown cancelled!
        }

        // manage backlight and display
        if ( abort == 0)
        {
            // update LED blink
            if (q==0)
            {
                debug_led(0);
                q=1;
            }
            else
            {
                debug_led(1);
                q=0;
            }

            // Display countdown of time to finish in mm: ss
            int s = (int)time_to_end;
            int minute =  s / 60;
            s = s % 60;
            int second = s;

            if ( (int)conf.gps_countdown==0 )
            {
                sprintf(vBuf, lang_str(LANG_MENU_GPS_t_6),minute, second);  // "Camera will wait for GPS for %01d:%02d"
            }
            else
            {
                sprintf(vBuf, " %01d:%02d",minute, second);
            }
            draw_txt_centered(0, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_RED));

            // Switch on the display when countdown <30 seconds
            if ((blite_off==1) && (time_to_end <=30))
            {
                TurnOnBackLight();
            }

            // start countdown if key pressed and backlight is off
            if ((blite_off==0) && (kbd_get_pressed_key() !=0))     // any key pressed ?
            {
                time_to_end=(int)conf.gps_wait_for_signal;
                o=1;
                p=1;
            }

            // start countdown and turn on backlight if key pressed and backlight is off
            if ((blite_off==1) && (kbd_get_pressed_key() !=0))     // any key pressed ?
            {
                TurnOnBackLight();
                blite_off=0;
                time_to_end=(int)conf.gps_wait_for_signal; 
                o=1;
                p=1;
            }

            // switch to play mode
            if ((o == (int)conf.gps_rec_play_time) && ((int)conf.gps_rec_play_set == 1))
            {
                if (camera_info.state.mode_rec)
                {
                    levent_set_play();
                }
            }

            // turn off backlight if timed out
            if ((p == (int)conf.gps_play_dark_time) && ((int)conf.gps_play_dark_set == 1))
            {
                if (camera_info.state.mode_play)
                {
                    TurnOffBackLight();
                    blite_off=1;
                }
            }
        }

        // tag waypoint reached

        lock(1) ;
            int l_stat      = g_d_stat ;
            double l_lat       = g_d_lat ;
            double l_lon       = g_d_lon ;
            double l_hei       = g_d_hei ;
            char l_tim[10];
            char l_dat[12];
            char l_tim_gps[10];
            char l_dat_gps[12];   
            strncpy( l_tim ,      g_d_tim,    10 );         
            strncpy( l_dat ,      g_d_dat,    12 );
            strncpy( l_tim_gps ,  g_d_tim_gps,10 );
            strncpy( l_dat_gps ,  g_d_dat_gps,12 );
        unlock(1);       
    
        if ( ((((int)conf.gps_2D_3D_fix) == 1) && (l_stat == 1)) || \
             ((((int)conf.gps_2D_3D_fix) == 1) && (l_stat == 2)) || \
             ((((int)conf.gps_2D_3D_fix) == 2) && (l_stat == 2)) || \
             ((((int)conf.gps_2D_3D_fix) == 3) && (l_stat == 2)) || \
             ((((int)conf.gps_2D_3D_fix) == 3) && (l_stat == 1) && (time_to_end < 3)) || \
             ((((int)conf.gps_2D_3D_fix) == 0) && (time_to_end < 3)) )
        {
            TurnOnBackLight();
            msleep (1000);

            fd0 = fopen ( "A/GPS/Tagging/wp.tmp", "r" );

            do
            {
                fgets (w1, 15, fd0);
                fseek (fd0, 1, SEEK_CUR);
                fgets (w2, 23, fd0);
                fseek (fd0, 1, SEEK_CUR);
                fgets (w3, 1, fd0);

                char lat[40];
                char lon[40];
                char hei[40];

                char vBuf1[512];
                char image_name[20];
                char wp_name[30];
                char gpx_name[30];
                char gpx_image_name[20];
                unsigned long t;
                static struct tm *ttm;
                t=time(NULL);
                ttm = localtime(&t);

                sprintf(wp_name, "A/GPS/Tagging/%04d_%02d_%02d.wp", ttm->tm_year+1900, ttm->tm_mon+1, ttm->tm_mday);
                sprintf(gpx_name, "A/GPS/Tagging/%04d_%02d_%02d.gpx", ttm->tm_year+1900, ttm->tm_mon+1, ttm->tm_mday);

                sprintf(image_name, "%s", w1);
                sprintf(gpx_image_name, "%s", w1);

                sprintf(vBuf1, "%s;%s;%s;%s;%s;%s",
                        image_name,
                        formatDouble (lat, l_lat, 0, 7),
                        formatDouble (lon, l_lon, 0, 7),
                        formatDouble (hei, l_hei, 0, 2),
                        w2,
                        w3);

                FILE* fp = fopen(wp_name, "a");

                if( fp )
                {
                    fwrite(vBuf1, strlen(vBuf1), 1, fp);
                }

                fclose(fp);

                fp = fopen(gpx_name, "r");
                if( fp == NULL)
                {
                    fp = fopen(gpx_name, "a");
                    if( fp )
                    {
                        sprintf(vBuf1, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
                        fwrite(vBuf1, strlen(vBuf1), 1, fp);
                        sprintf(vBuf1, "<gpx xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" version=\"1.1\" creator=\"CHDK\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n");
                        fwrite(vBuf1, strlen(vBuf1), 1, fp);
                        sprintf(vBuf1, "</gpx>");
                        fwrite(vBuf1, strlen(vBuf1), 1, fp);
                        fclose(fp);
                    }
                }


                fp = fopen(gpx_name, "a");
                if( fp )
                {
                    fseek (fp, -6, SEEK_END);

                    sprintf(vBuf1, "   <wpt lat=\"%s\" lon=\"%s\">\n",formatDouble (lat, l_lat, 0, 7), formatDouble (lon, l_lon, 0, 7));
                    fwrite(vBuf1, strlen(vBuf1), 1, fp);
                    sprintf(vBuf1, "    <ele>%s</ele>\n",formatDouble (hei, l_hei, 0, 2));
                    fwrite(vBuf1, strlen(vBuf1), 1, fp);
                    sprintf(vBuf1, "    <time>%s</time>\n", w2);
                    fwrite(vBuf1, strlen(vBuf1), 1, fp);
                    sprintf(vBuf1, "    <gpst>%sT%sZ</gpst>\n", l_dat_gps, l_tim_gps);
                    fwrite(vBuf1, strlen(vBuf1), 1, fp);
                    sprintf(vBuf1, "    <temp>%i</temp>\n", (int)get_optical_temp());
                    fwrite(vBuf1, strlen(vBuf1), 1, fp);
                    sprintf(vBuf1, "    <name>%s</name>\n", gpx_image_name);
                    fwrite(vBuf1, strlen(vBuf1), 1, fp);
                    sprintf(vBuf1, "   </wpt>\n");
                    fwrite(vBuf1, strlen(vBuf1), 1, fp);
                    sprintf(vBuf1, "</gpx>");
                    fwrite(vBuf1, strlen(vBuf1), 1, fp);
                }
                fclose(fp);
            }
            while (!feof(fd0));

            fclose (fd0);
            if ( abort == 0)
            {
                time_to_end=1;
            }

        }

        msleep(1000);
        o++;
        p++;


        // gps functionality disabled ?
        if ((int)conf.gps_on_off ==0)
        {
            debug_led(0);
            gps_nosignal_lock = 0;             
            ExitTask();
        }

        time_to_end--;
    }

    int zba, zba1;

    if (abort == 0)
    {
        for(zba=15; zba>0; zba--)           // outer loop = 30 seconds
        {
            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_4), zba);     // "Camera will shutdown in %02d seconds!"
            draw_txt_centered(2, vBuf, MAKE_COLOR(COLOR_WHITE, COLOR_BLUE));
            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_18));         //"To cancel [Press half]"
            draw_txt_centered(3, vBuf, MAKE_COLOR(COLOR_WHITE, COLOR_RED));
            play_sound(6);

            for(zba1=100; zba1>0; zba1--)   // inner loop = 2 seconds
            {
                if (kbd_get_pressed_key() == KEY_SHOOT_HALF)
                    {
                        TurnOnBackLight();
                        debug_led(0);
                        gps_nosignal_lock = 0 ; 
                        ExitTask();
                    }
                msleep(2);
            }
        }        
        camera_shutdown_in_a_second();
    }

    TurnOnBackLight();

    // beep 5 seconds
    if ((int)conf.gps_beep_warn == 1)
    {
        for(zba=5; zba>0; zba--) play_sound(6);
    }
    
    debug_led(0);
    gps_nosignal_lock = 0 ; 
    ExitTask();

}


//=======================================================================
//
// TASK : started by init_gps_logging_task() to record logging data
//
static void gps_logging_task(){
      
    char lat[40];
    char lon[40];
    char hei[40];
    char vBuf[512];
    unsigned long t;
    char gpx_name[30];
    static struct tm *ttm;
    t=time(NULL);
    ttm = localtime(&t);

    mkdir_if_not_exist("A/GPS");
    mkdir_if_not_exist("A/GPS/Logging");
    sprintf(gpx_name, "A/GPS/Logging/%02d_%02d-%02d_%02d.gpx", ttm->tm_mon+1, ttm->tm_mday, ttm->tm_hour, ttm->tm_min);

    FILE* fp = fopen(gpx_name, "w");
    if( fp )
    {
        sprintf(vBuf, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
        fwrite(vBuf, strlen(vBuf), 1, fp);
        sprintf(vBuf, "<gpx xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" version=\"1.1\" creator=\"CHDK\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n");
        fwrite(vBuf, strlen(vBuf), 1, fp);
        sprintf(vBuf, " <metadata />\n");
        fwrite(vBuf, strlen(vBuf), 1, fp);
        sprintf(vBuf, " <trk>\n");
        fwrite(vBuf, strlen(vBuf), 1, fp);
        sprintf(vBuf, "  <name />\n");
        fwrite(vBuf, strlen(vBuf), 1, fp);
        sprintf(vBuf, "  <cmt />\n");
        fwrite(vBuf, strlen(vBuf), 1, fp);
        sprintf(vBuf, "  <trkseg>\n");
        fwrite(vBuf, strlen(vBuf), 1, fp);
        sprintf(vBuf, "  </trkseg>\n");
        fwrite(vBuf, strlen(vBuf), 1, fp);
        sprintf(vBuf, " </trk>\n");
        fwrite(vBuf, strlen(vBuf), 1, fp);
        sprintf(vBuf, "</gpx>\n");
        fwrite(vBuf, strlen(vBuf), 1, fp);
    }
    fclose(fp);
    int zae=1, zae_1=0, bat;
    
    while ( exit_record_task==0 )
    {
        if (kbd_get_pressed_key() !=0)   // any key pressed ?
        {
            TurnOnBackLight();
            zae_1=0;
        }

        zae_1++;
        if ((zae_1 == (int)conf.gps_rec_play_time_1) && ((int)conf.gps_rec_play_set_1 == 1))
        {
            if (camera_info.state.mode_rec)
            {
                levent_set_play();
            }
        }
        if ((zae_1 == (int)conf.gps_play_dark_time_1) && ((int)conf.gps_play_dark_set_1 == 1))
        {
            if (camera_info.state.mode_play)
            {
                TurnOffBackLight();
            }
        }
        if ( (((zae_1) % 2)) == 0 )
        {
            debug_led(0);
        }
        else
        {
            debug_led(1);
        }
        if ( (((zae * (int)conf.gps_track_time) % 60)) == 0 )
        {
            bat=get_batt_perc();
            if (bat <= (int)conf.gps_batt)
            {
                int zba;
                for(zba=30; zba>0; zba--)
                {
                    sprintf(vBuf, lang_str(LANG_MENU_GPS_t_3));  // "Battery below setting!"
                    draw_txt_centered(8, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_RED));
                    sprintf(vBuf, lang_str(LANG_MENU_GPS_t_4),zba);
                    draw_txt_centered(9, vBuf, MAKE_COLOR(COLOR_GREY_DK_TRANS, COLOR_RED));
                    if ( (((zba) % 2)) == 0 )
                    {
                        debug_led(0);
                        play_sound(6);
                    }
                    else
                    {
                        debug_led(1);
                    }
                    msleep(1000);
                }
                camera_shutdown_in_a_second();
                gps_record_lock = 0 ; 
                ExitTask();
            }
        }
        
        lock(2) ;
            int l_stat   = g_d_stat ;
            double l_lat = g_d_lat ;
            double l_lon = g_d_lon ;
            double l_hei = g_d_hei ;
            char l_tim[10];
            char l_dat[12];  
            strncpy( l_tim, g_d_tim, 10 );         
            strncpy( l_dat, g_d_dat, 12 );          
        unlock(2) ;
        
        if (l_stat == 1 || l_stat == 2)
        {
            fp = fopen(gpx_name, "a");
            if( fp )
            {
                fseek (fp, -27, SEEK_END);
                sprintf(vBuf, "   <trkpt lat=\"%s\" lon=\"%s\">\n",formatDouble (lat, l_lat, 0, 7), formatDouble (lon, l_lon, 0, 7));
                fwrite(vBuf, strlen(vBuf), 1, fp);
                sprintf(vBuf, "    <ele>%s</ele>\n",formatDouble (hei, l_hei, 0, 2));
                fwrite(vBuf, strlen(vBuf), 1, fp);
                sprintf(vBuf, "    <time>%sT%sZ</time>\n", l_dat, l_tim);
                fwrite(vBuf, strlen(vBuf), 1, fp);
                sprintf(vBuf, "   </trkpt>\n");
                fwrite(vBuf, strlen(vBuf), 1, fp);
                sprintf(vBuf, "  </trkseg>\n");
                fwrite(vBuf, strlen(vBuf), 1, fp);
                sprintf(vBuf, " </trk>\n");
                fwrite(vBuf, strlen(vBuf), 1, fp);
                sprintf(vBuf, "</gpx>\n");
                fwrite(vBuf, strlen(vBuf), 1, fp);
            }
            fclose(fp);
        }
        msleep((int)conf.gps_track_time*1000);
        zae++;
    }
    debug_led(0);
    gps_record_lock = 0 ; 
    ExitTask();

}

//=======================================================================
//
// TASK : compass GUI display
//
static void compass_display_task(){

    #define BITMAP_WIDTH  61
    #define BITMAP_HEIGHT 61

    georegInit (&georeg, (int)conf.gps_compass_smooth, buffer1, buffer2);
    regressionInit (&deltareg, 20, buffer3);

    int stat ;
    double angle=0;
    double w=0.0;
    double c_w;
    double s_w;
    int n=0;
    int count=0;
    int m=0;
    int old_angle=0;

    int offset_x = 32;
    int m_x = offset_x + 31;
    int offset_y = 150;     //128
    int m_y = offset_y + 31;

    int f_v_0=COLOR_TRANSPARENT;
//     int f_h_0=COLOR_TRANSPARENT;

    int f_v_1=COLOR_BLUE;
//     int f_h_1=COLOR_BLUE;

    int f_v_2= COLOR_WHITE ;
//     int f_h_2= COLOR_WHITE ;

    int f_v_3= COLOR_BLACK ;
//     int f_h_3= COLOR_BLACK ;

    int f_v_4= COLOR_BLACK ;
//     int f_h_4= COLOR_BLACK ;

    double old_c_w=cos(0);
    double old_s_w=sin(0);

    char *bitmap1 = load_bitmap("A/CHDK/DATA/GPS_needle.txt");
    char *bitmap2 = load_bitmap("A/CHDK/DATA/GPS_compass.txt");
    int extent = strlen(bitmap1);

    while (exit_compass_task == 0) 
    {
        lock(3);
            angle= draw_gps_course_info(count);
            stat = g_d_stat ;
        unlock(3);
        
        count++;
        m=n;
        
        if ((int)angle != 0) { m=angle; }

        switch ( stat )
        {
            case 0 : 
                f_v_1= COLOR_RED ;
//                 f_h_1= COLOR_RED ;
                break ;
            case 1 :
                f_v_1= COLOR_YELLOW ;
//                 f_h_1= COLOR_YELLOW ;
                break ;
            case 2 :
                f_v_1= COLOR_GREEN ;
//                 f_h_1= COLOR_GREEN ;
                break ;
        }
        
        if (camera_info.state.gui_mode_none)
        {
            draw_compass (bitmap2, offset_x - 27, offset_y -14, f_v_0, f_v_1, f_v_2, f_v_4);
        }
        if (stat == 2 )
        {
            f_v_1= COLOR_BLUE ;
//             f_h_1= COLOR_BLUE ;
        }
        if (m>=0 && m<180)
        {
            w=(180-m)*3.141592653589793/180;
        }
        if (m >=180 && m<=360)
        {
            w=(540-m)*3.141592653589793/180;
        }

        c_w=cos(w);
        s_w=sin(w);

        if (camera_info.state.gui_mode_none)
        {
            draw_compass_needle (old_angle, old_s_w, old_c_w, bitmap1, extent, m_x, m_y, offset_x, offset_y, f_v_0, f_v_0);
            draw_compass_needle (m, s_w, c_w, bitmap1, extent, m_x, m_y, offset_x, offset_y, f_v_1, f_v_3);
        }
        old_angle=m;
        old_c_w=cos(w);
        old_s_w=sin(w);
        msleep((int)conf.gps_compass_time*1000);
    }
    if (bitmap1) free(bitmap1) ;
    if (bitmap2) free(bitmap2) ;
    gps_compass_lock = 0 ;     
    ExitTask();
}


//=======================================================================
//
//  Functions called from outside this module :
//      - GUI menu support functions
//      - background tasks
//

// gps_waypoint()
//  - called from spytask after each shot (if gps_waypoint_save is set)
//  - logs current gpx data if available, otherwise starts gpx_no_signal task to wait for data
void gps_waypoint(){     
    
    char lat[40];
    char lon[40];
    char hei[40];

    char vBuf[512];
    char image_name[20];
    char wp_name[30];
    char gpx_name[30];
    char gpx_image_name[20];

    unsigned long t;
    static struct tm *ttm;
    t=time(NULL);
    ttm = localtime(&t);

    mkdir_if_not_exist("A/GPS");
    mkdir_if_not_exist("A/GPS/Tagging");
    sprintf(wp_name, "A/GPS/Tagging/%04d_%02d_%02d.wp", ttm->tm_year+1900, ttm->tm_mon+1, ttm->tm_mday);
    sprintf(gpx_name, "A/GPS/Tagging/%04d_%02d_%02d.gpx", ttm->tm_year+1900, ttm->tm_mon+1, ttm->tm_mday);
    sprintf(image_name, "IMG_%04d.JPG", get_target_file_num());

    lock(4);
        int l_stat         = g_d_stat ;
        double l_lat       = g_d_lat ;
        double l_lon       = g_d_lon ;
        double l_hei       = g_d_hei ;
        char l_tim[10];
        char l_dat[12];
        char l_tim_gps[10];
        char l_dat_gps[12];   
        strncpy( l_tim ,      g_d_tim,     10 );
        strncpy( l_dat ,      g_d_dat,     12 );
        strncpy( l_tim_gps ,  g_d_tim_gps, 10 );
        strncpy( l_dat_gps ,  g_d_dat_gps, 12 );
    unlock(4);
        
    if ((l_stat >= ((int)conf.gps_2D_3D_fix)) || ((((int)conf.gps_2D_3D_fix) == 3) && (l_stat == 2)))
    {
        sprintf(vBuf, "%s;%s;%s;%s;%sT%sZ;%i\n",
                image_name,
                formatDouble (lat, l_lat, 0, 7),
                formatDouble (lon, l_lon, 0, 7),
                formatDouble (hei, l_hei, 0, 2),
                l_dat, l_tim,
                get_optical_temp());

        FILE* fp = fopen(wp_name, "a");

        if( fp )
        {
            fwrite(vBuf, strlen(vBuf), 1, fp);
        }
        fclose(fp);

        fp = fopen(gpx_name, "r");
        if( fp == NULL)
        {
            fp = fopen(gpx_name, "a");
            if( fp )
            {
                sprintf(vBuf, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
                fwrite(vBuf, strlen(vBuf), 1, fp);
                sprintf(vBuf, "<gpx xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" version=\"1.1\" creator=\"CHDK\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n");
                fwrite(vBuf, strlen(vBuf), 1, fp);
                sprintf(vBuf, "</gpx>");
                fwrite(vBuf, strlen(vBuf), 1, fp);
                fclose(fp);
            }
        }


        sprintf(gpx_image_name, "IMG_%04d.JPG", get_target_file_num());

        fp = fopen(gpx_name, "a");
        if( fp )
        {
            fseek (fp, -6, SEEK_END);

            sprintf(vBuf, "   <wpt lat=\"%s\" lon=\"%s\">\n",formatDouble (lat, l_lat, 0, 7), formatDouble (lon, l_lon, 0, 7));
            fwrite(vBuf, strlen(vBuf), 1, fp);
            sprintf(vBuf, "    <ele>%s</ele>\n",formatDouble (hei, l_hei, 0, 2));
            fwrite(vBuf, strlen(vBuf), 1, fp);
            sprintf(vBuf, "    <time>%sT%sZ</time>\n", l_dat, l_tim);
            fwrite(vBuf, strlen(vBuf), 1, fp);
            sprintf(vBuf, "    <gpst>%sT%sZ</gpst>\n", l_dat_gps, l_tim_gps);
            fwrite(vBuf, strlen(vBuf), 1, fp);
            sprintf(vBuf, "    <temp>%i</temp>\n", (int)get_optical_temp());
            fwrite(vBuf, strlen(vBuf), 1, fp);
            sprintf(vBuf, "    <name>%s</name>\n", gpx_image_name);
            fwrite(vBuf, strlen(vBuf), 1, fp);
            sprintf(vBuf, "   </wpt>\n");
            fwrite(vBuf, strlen(vBuf), 1, fp);
            sprintf(vBuf, "</gpx>");
            fwrite(vBuf, strlen(vBuf), 1, fp);
        }
        fclose(fp);
    }
    else
    {
         FILE* fp ;
         
        sprintf(vBuf, "%s %sT%sZ %i\n",
                image_name,
                l_dat, l_tim,
                get_optical_temp());
          
        if ( gps_nosignal_lock == 0 )
        {              
            gps_nosignal_lock = 1 ;            
            CreateTask("GPS_NOSIGNAL", 0x19, 0x1200, no_signal_task);
            fp = fopen("A/GPS/Tagging/wp.tmp", "w");
        }
        else
        {
            fp = fopen("A/GPS/Tagging/wp.tmp", "a");
        }
        


        if( fp )
        {
            fwrite(vBuf, strlen(vBuf), 1, fp);
            fclose(fp);
        }
        
        time_to_end=(int)conf.gps_wait_for_signal;
    }
        
}



void gps_write_timezone(){          // called from gui.c when "Set position as current timezone" is selected in the UI
     
    char vBuf[30];
    char timezone_name[30];

    lock(5);                      // note : kbd task lock = 5
        int l_stat   = g_d_stat ;
        double l_lat = g_d_lat ;
        double l_lon = g_d_lon ;
    unlock(5);
    
    if (l_stat !=0)
    {
        mkdir_if_not_exist("A/GPS");
        mkdir_if_not_exist("A/GPS/Timezone");
        sprintf(timezone_name, "A/GPS/Timezone/Timezone.txt");
        
        FILE* fp = fopen(timezone_name, "w");
        if( fp )
        {
            sprintf(vBuf, "%i\n", (long)(l_lat*10000000));
            fwrite(vBuf, strlen(vBuf), 1, fp);
            sprintf(vBuf, "%i\n", (long)(l_lon*10000000));
            fwrite(vBuf, strlen(vBuf), 1, fp);
        }
        fclose(fp);
    }
    else
    {
        gui_mbox_init(LANG_ERROR, LANG_MENU_GPS_t_1, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL); //No GPS!
    }
}

void gps_write_home(){             // called from gui.c when "Set position as home location" is selected in the UI
    
    char vBuf[30];
    char home_name[30];

    lock(5);                       // note : kbd task lock = 5
        int l_stat = g_d_stat ;
        double l_lat = g_d_lat ;
        double l_lon = g_d_lon ;
    unlock(5);
        

    if (l_stat !=0)
    {
        mkdir_if_not_exist("A/GPS");
        mkdir_if_not_exist("A/GPS/Navigation");
        sprintf(home_name, "A/GPS/Navigation/Home.txt");

        FILE* fp = fopen(home_name, "w");
        if( fp )
        {
            sprintf(vBuf, "%i\n", (long)(l_lat*10000000));
            fwrite(vBuf, strlen(vBuf), 1, fp);
            sprintf(vBuf, "%i\n", (long)(l_lon*10000000));
            fwrite(vBuf, strlen(vBuf), 1, fp);
        }
        fclose(fp);
    }
    else
    {
        gui_mbox_init(LANG_ERROR, LANG_MENU_GPS_t_1, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL); //No GPS!
    }
}

void init_gps_startup(int stop_request)                       // called from spytask
{
    if ( stop_request == 0 )
    {
         if ( gps_data_lock == 0) 
        {
            exit_data_task = 0;             
            gps_data_lock = 1 ;            
            CreateTask("GPS_DATA", 0x19, 0x800, gps_data_task);
        }
    }
    else 
    {
        if ( exit_data_task != 1)
        {
            exit_data_task =1;        
            exit_show_task=1;
            exit_record_task=1;
            exit_track_task=1;
            exit_compass_task=1;
            exit_nosignal_task=1;        
        }
    }
}

void init_gps_logging_task(int stop_request){               // called from gui.c to start the gpx logging
    exit_track_task = exit_record_task = stop_request ;
    
    if ( stop_request == 0 ) 
    {
        if (gps_record_lock == 0)
        {
            gps_record_lock  = 1 ;                
            CreateTask("GPS_RECORD", 0x19, 0x800, gps_logging_task);
        }

        if (((int)conf.gps_track_symbol==1) && (gps_track_lock == 0))
        {
            gps_track_lock = 1 ;                
            CreateTask("GPS_TRACK", 0x19, 0x800, tracking_icon_task);

        }
    } ; 
}

void init_gps_compass_task(int stop_request){               // called from gui.c to start the GUI compass display
        
    exit_compass_task = stop_request ; 
    if ( stop_request == 0 )
    {     
        run_compass_task(0);
    }   
}


int init_gps_navigate_to_home(int stop_request){            // called from gui.c when navigate home selected from GUI

    exit_compass_task = stop_request ;
    if ( stop_request == 0 )
    {      
        char text[2][30];
        char home_name[30];
        char * endptr;

        sprintf(home_name, "A/GPS/Navigation/Home.txt");

        FILE* fp = fopen(home_name, "r");
        if( fp )
        {
            fgets(text[1], 15, fp);
            fgets(text[2], 15, fp);
        }
        fclose(fp);

        g_d_lat_nav = (strtol (text[1], &endptr, 10 )) / 10000000.0;
        g_d_lon_nav = (strtol (text[2], &endptr, 10 )) / 10000000.0;

        if ((int)g_d_lat_nav != 0)
        {        
            run_compass_task(2);          
            return(1);
        }
        else
        {
            gui_mbox_init(LANG_ERROR, LANG_MENU_GPS_t_7, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL); //Navigation to Home Loc is not possible!
            navigation_mode=0;
        }
    }
    return(0);
}

int init_gps_navigate_to_photo(int stop_request){                  // called from gui.c when show navi selected by GUI

    exit_compass_task = stop_request ; 

    if (stop_request == 0)
    {      
        gps_img_data *igps = ( gps_img_data *) camera_jpeg_current_gps();

        g_d_lat_nav=0.0;
        g_d_lon_nav=0.0;

        char image[9];
        strncpy(image, camera_jpeg_current_filename(),8) ;
        image[8] = '\0';

        if (igps->latitudeRef[0] && igps->longitudeRef[0])
        {
            g_d_lat_nav=(double)igps->latitude[0]/(double)igps->latitude[1]*1.0 +
                                 igps->latitude[2]/(igps->latitude[3]*60.0) +
                                 igps->latitude[4]/(igps->latitude[5]*3600.0);
            if (igps->latitudeRef[0] == 'S') g_d_lat_nav = -g_d_lat_nav;
            g_d_lon_nav=(double)igps->longitude[0]/(double)igps->longitude[1]*1.0 +
                                 igps->longitude[2]/(igps->longitude[3]*60.0) +
                                 igps->longitude[4]/(igps->longitude[5]*3600.0);
            if (igps->longitudeRef[0] == 'W') g_d_lon_nav = -g_d_lon_nav;
             
            run_compass_task(1);
            return(1);
        }
        else
        {
            char vBuf[256];
            sprintf(vBuf, lang_str(LANG_MENU_GPS_t_8), image);  //"Cant navigate to photo: %s!"
            gui_mbox_init(LANG_ERROR, (int)vBuf, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL); //Cant navigate to photo: %s!
            navigation_mode=0;
        }
    }   
    return(0) ;
}

/****  eof  ****/
