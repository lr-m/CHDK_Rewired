/*
 * 
 * GPS simulation code for cameras without GPS capability
 *   - only useful for testing code changes on non-GPS cameras
 *   - comment out gps_key_trap, camera_jpeg_current_filename, camera_jpeg_current_gps here if 
 *     they are actually defined for the camera being tested
 * 
 */
 
char gps_fakename[32] = "img_1234" ;
tGPS gps_dummy_data ;

char *camera_jpeg_current_filename() { return (void*)gps_fakename; }
char *camera_jpeg_current_gps() { return (void*)&gps_dummy_data; }

static int gps_first_pass = 0 ;
static int x_velocity = 4000 ;
static int y_velocity = 2000 ;

// Define to prevent compile error when testing simulated GPS
void __attribute__((weak)) _GPS_UpdateData() {}

void GPS_FakeData()
{
    struct tm *ttm = get_localtime();

    gps_dummy_data.timeStamp[0] = ttm->tm_hour;
    gps_dummy_data.timeStamp[1] = 1 ;
    gps_dummy_data.timeStamp[2] = ttm->tm_min ;
    gps_dummy_data.timeStamp[3] = 1 ;
    gps_dummy_data.timeStamp[4] = ttm->tm_sec*1000 ;
    gps_dummy_data.timeStamp[5] = 1000 ;    
    
    
    if ( gps_first_pass == 0 ) 
    {
        gps_first_pass = 1; 
        
        gps_dummy_data.latitude[0] = 40;
        gps_dummy_data.latitude[1] = 1;
        gps_dummy_data.latitude[2] = 00;
        gps_dummy_data.latitude[3] = 1;
        gps_dummy_data.latitude[4] = 0;
        gps_dummy_data.latitude[5] = 1000;
        
        gps_dummy_data.longitude[0] = 80 ;
        gps_dummy_data.longitude[1] = 1 ;
        gps_dummy_data.longitude[2] = 0 ;
        gps_dummy_data.longitude[3] = 1;
        gps_dummy_data.longitude[4] = 0;
        gps_dummy_data.longitude[5] = 1000;
        
        gps_dummy_data.height[0] = 3000 ;
        gps_dummy_data.height[1] = 100 ;
                    
        strcpy(gps_dummy_data.latitudeRef ,"N"  ) ;
        strcpy(gps_dummy_data.longitudeRef ,"W" ) ;
        strcpy(gps_dummy_data.heightRef,   "n/a") ;   // not used
        strcpy(gps_dummy_data.status,      "??" ) ;   // not used
        strcpy(gps_dummy_data.mapDatum,    "??" ) ;   // not used
        strcpy(gps_dummy_data.dateStamp,   "??" ) ;   // not used
        strcpy(gps_dummy_data.unknown2,    "???") ;   // not used
    }
    else
    {
        gps_dummy_data.latitude[4] +=  x_velocity;
        if( gps_dummy_data.latitude[4] >= 60000 )
        {
            gps_dummy_data.latitude[4] = gps_dummy_data.latitude[4] - 60000;
            if( ++gps_dummy_data.latitude[2] >= 60 ) 
            {
                gps_dummy_data.latitude[2] = 0 ;
                if( ++gps_dummy_data.latitude[0] >= 42 ) x_velocity *= -1 ;
            }
        }
        if( gps_dummy_data.latitude[4] < 0 )
        {
            gps_dummy_data.latitude[4]  = gps_dummy_data.latitude[4] + 60000 ;
            if ( --gps_dummy_data.latitude[2] < 0 )        
            {
                gps_dummy_data.latitude[2] = 59 ;              
                if( --gps_dummy_data.latitude[0] < 40 ) x_velocity *= -1 ;
            }
        }
     
        gps_dummy_data.longitude[4] +=  y_velocity;
        if( gps_dummy_data.longitude[4] >= 60000 )
        {
            gps_dummy_data.longitude[4] = gps_dummy_data.longitude[4] - 60000;
            if( ++gps_dummy_data.longitude[2] >= 60 ) 
            {
                gps_dummy_data.longitude[2] = 0 ;
                if( ++gps_dummy_data.longitude[0] >= 82 ) y_velocity *= -1 ;
            }
        }
        if( gps_dummy_data.longitude[4] < 0 )
        {
            gps_dummy_data.longitude[4]  = gps_dummy_data.longitude[4] + 60000 ;
            if ( --gps_dummy_data.longitude[2] < 0 )        
            {
                gps_dummy_data.longitude[2] = 59 ;              
                if( --gps_dummy_data.longitude[0] < 80 ) y_velocity *= -1 ;
            }
        }
    }
    
    char vBuf[256];
    sprintf(vBuf, "%02d.%02d.%05d  %02d.%02d.%05d",
        gps_dummy_data.latitude[0] / gps_dummy_data.latitude[1] , 
        gps_dummy_data.latitude[2] / gps_dummy_data.latitude[3] ,
        gps_dummy_data.latitude[4] / gps_dummy_data.latitude[5] ,   
        gps_dummy_data.longitude[0] / gps_dummy_data.longitude[1] , 
        gps_dummy_data.longitude[2] / gps_dummy_data.longitude[3] ,
        gps_dummy_data.longitude[4] / gps_dummy_data.longitude[5] );      
    draw_txt_string(1, 0, vBuf, MAKE_COLOR(COLOR_TRANSPARENT, COLOR_RED));    
    
    return ;
} ;
