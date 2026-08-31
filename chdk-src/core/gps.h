#ifndef __CHDK_GPS_H
#define __CHDK_GPS_H

// CHDK GPS interface

extern char * camera_jpeg_current_filename();
extern char * camera_jpeg_current_gps() ;
extern void   GPS_UpdateData();
extern int    gps_key_trap ;

extern int init_gps_navigate_to_photo(int  );
extern int init_gps_navigate_to_home(int);
extern void init_gps_compass_task(int );
extern void init_gps_logging_task(int );
extern void init_gps_startup(int ); 
extern void gps_write_home();
extern void gps_write_timezone();
extern void gps_waypoint();

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

typedef struct {
    char    latitudeRef[4];
    int     latitude[6];
    char    longitudeRef[4];
    int     longitude[6];
    char    heightRef[4];
    int     height[2];
    int     timeStamp[6];
    char    status[2];
    char    mapDatum[7];
    char    dateStamp[11];
    char    unknown2[160];
} tGPS;

//current image in play mode
typedef struct {
    char    latitudeRef[4];
    int     latitude[6];
    char    longitudeRef[4];
    int     longitude[6];
    char    heightRef[4];
    int     height[2];
    int     timeStamp[6];
    char    dateStamp[11];
} gps_img_data;

#endif
