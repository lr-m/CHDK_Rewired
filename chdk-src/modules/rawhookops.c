/*
functions for operating on raw framebuffer from script hooks
*/
#include "camera_info.h"
#include "conf.h"
#include "raw.h"
#include "math.h"

#include "lualib.h"
#include "lauxlib.h"

extern void set_number_field(lua_State *L, const char *name, int value);

// set when in hook and capture mode supports raw
static int raw_buffer_valid;

// TODO not really the same for R,G,B
// raw value of a neutral exposure, including black level
static unsigned raw_neutral;
// log2(raw_neutral - blacklevel), i.e. the range of significant raw values
static double log2_raw_neutral_count; 

// offsets of bayer elements from an even pixel coordinate
// [r,g,g,b][x,y]
static unsigned cfa_offsets[4][2];
static const char *cfa_names[]={"r","g1","g2","b"};
#define CFA_R 0
#define CFA_G1 1
#define CFA_G2 2
#define CFA_B 3
#define RAWOP_HISTO_META "rawop.histo_meta"

// simple round half away from zero
static int round_d2i(double v) {
    if(v<0.0) {
        return (int)(v - 0.5);
    }
    return (int)(v + 0.5);
}
/*
cfa=rawop.get_cfa()
return: CFA pattern as a 32 bit interger, as used in DNG
*/
static int rawop_get_cfa(lua_State *L) {
    lua_pushnumber(L,camera_sensor.cfa_pattern);
    return 1;
}

/*
cfa_offsets=rawop.get_cfa_offsets()
returns: offsets of color filter elements with respect to an even valued x,y pair, in the form
{
 r={ y=1, x=0, },
 g1={ y=0, x=0, },
 b={ y=0, x=1, },
 g2={ y=1, x=1, },
}
*/
static int rawop_get_cfa_offsets(lua_State *L) {
    lua_createtable(L, 0, 4);
    int i;
    for(i=0;i<4;i++) {
        lua_createtable(L, 0, 2);
        set_number_field(L,"x",cfa_offsets[i][0]);
        set_number_field(L,"y",cfa_offsets[i][1]);
        lua_setfield(L, -2, cfa_names[i]);
    }
    return 1;
}

/*
bpp=rawop.get_bits_per_pixel()
returns: sensor bit depth (10, 12, or 14 in currently supported cameras)
*/
static int rawop_get_bits_per_pixel(lua_State *L) {
    lua_pushnumber(L,camera_sensor.bits_per_pixel);
    return 1;
}

/*
neutral=rawop.get_raw_neutral()
returns: approximate raw value of a neutral grey target exposed with canon AE
raw_to_ev96(netural) = 0
This is not aware of CFA, it is based on an average of all color elements

NOTE on G1x, the value may change depending on ISO settings. The value is only
updated when the raw hook is entered for the shot so for maximum portability,
this function should only be called inside the raw hook.
*/
static int rawop_get_raw_neutral(lua_State *L) {
    lua_pushnumber(L,(int)raw_neutral);
    return 1;
}

/*
bl=rawop.get_black_level()
returns: sensor black level value

NOTE on G1x, the value may change depending on ISO settings. The value is only
updated when the raw hook is entered for the shot so for maximum portability,
this function should only be called inside the raw hook.
*/
static int rawop_get_black_level(lua_State *L) {
    lua_pushnumber(L,camera_sensor.black_level);
    return 1;
}

/*
wl=rawop.get_white_level()
returns: sensor white level value (2^bpp - 1 for all known sensors)
*/
static int rawop_get_white_level(lua_State *L) {
    lua_pushnumber(L,camera_sensor.white_level);
    return 1;
}

/*
w=rawop.get_raw_width()
returns: width of the raw buffer, in pixels
*/
static int rawop_get_raw_width(lua_State *L) {
    lua_pushnumber(L,camera_sensor.raw_rowpix);
    return 1;
}

/*
h=rawop.get_raw_height()
returns height of the raw buffer, in pixels
*/
static int rawop_get_raw_height(lua_State *L) {
    lua_pushnumber(L,camera_sensor.raw_rows);
    return 1;
}

/*
active area functions

NOTES
Active area is defined in the port, and is not affected by the "Crop size" DNG menu option

Active area may include dark borders of pixels which contain image data, but are not exposed
the same as the majority of the sensor. JPEG area may be a better choice for whole scene
measurements.
*/

/*
left=rawop.get_active_left()
returns: x coordinate of the leftmost pixel containing valid data in the raw buffer
*/
static int rawop_get_active_left(lua_State *L) {
    lua_pushnumber(L,camera_sensor.active_area.x1);
    return 1;
}

/*
top=rawop.get_active_top()
returns: y coordinate of the topmost pixel containing valid data in the raw buffer
*/
static int rawop_get_active_top(lua_State *L) {
    lua_pushnumber(L,camera_sensor.active_area.y1);
    return 1;
}

/*
w=rawop.get_active_width()
returns: width of the active area, in pixels
*/
static int rawop_get_active_width(lua_State *L) {
    lua_pushnumber(L,camera_sensor.active_area.x2 - camera_sensor.active_area.x1);
    return 1;
}

/*
h=rawop.get_active_height()
returns: height of the active area, in pixels
*/
static int rawop_get_active_height(lua_State *L) {
    lua_pushnumber(L,camera_sensor.active_area.y2 - camera_sensor.active_area.y1);
    return 1;
}

/*
JPEG area functions

NOTES
JPEG area is defined in the port, and is not affected by the "Crop size" DNG menu option

JPEG area represents the approximate area of the sensor used for the JPEG, but may not
exactly match either the pixel dimensions or sensor area.

These functions return sensor coordinates, not active area relative coordinates used
in DNG metadata.

JPEG area generally will not include the dark borders that can affect active area, so jpeg
area is a good choice for measuring the whole scene.
*/

/*
left=rawop.get_jpeg_left()
returns: x coordinate of the leftmost pixel of the jpeg area, in sensor coordinates
*/
static int rawop_get_jpeg_left(lua_State *L) {
    lua_pushnumber(L,camera_sensor.active_area.x1 + camera_sensor.jpeg.x);
    return 1;
}

/*
top=rawop.get_jpeg_top()
returns: y coordinate of the topmost pixel of the jpeg area, in sensor coordinates
*/
static int rawop_get_jpeg_top(lua_State *L) {
    lua_pushnumber(L,camera_sensor.active_area.y1 + camera_sensor.jpeg.y);
    return 1;
}

/*
width=rawop.get_jpeg_width()
returns: width of the jpeg area, in pixels
*/
static int rawop_get_jpeg_width(lua_State *L) {
    lua_pushnumber(L,camera_sensor.jpeg.width);
    return 1;
}

/*
height=rawop.get_jpeg_height()
returns: height of the jpeg area, in pixels
*/
static int rawop_get_jpeg_height(lua_State *L) {
    lua_pushnumber(L,camera_sensor.jpeg.height);
    return 1;
}


/*
raw buffer access functions
*/
/*
v=rawop.get_pixel(x,y)
return: raw value, or nil if out of bounds

An error is generated if the this function is called outside the raw hook, or in a shooting
mode for which raw data is not available.
*/
static int rawop_get_pixel(lua_State *L) {
    if(!raw_buffer_valid) {
        return luaL_error(L,"raw data not available");
    }
    unsigned x=luaL_checknumber(L,1);
    unsigned y=luaL_checknumber(L,2);
    // TODO return nil for out of bounds?
    // might not want to check, or return 0, or error()?
    if(x >= (unsigned)camera_sensor.raw_rowpix || y >= (unsigned)camera_sensor.raw_rows) {
        return 0;
    }
    lua_pushnumber(L,get_raw_pixel(x,y));
    return 1;
}

/*
rawop.set_pixel(x,y,v)
sets pixel to v

An error is generated if the this function is called outside the raw hook, or in a shooting
mode for which raw data is not available.
*/
static int rawop_set_pixel(lua_State *L) {
    if(!raw_buffer_valid) {
        return luaL_error(L,"raw data not available");
    }
    unsigned int x=luaL_checknumber(L,1);
    unsigned int y=luaL_checknumber(L,2);
    unsigned short v=luaL_checknumber(L,3);
    // TODO
    // might want to or error()?
    if(x >= (unsigned)camera_sensor.raw_rowpix || y >= (unsigned)camera_sensor.raw_rows) {
        return 0;
    }
    // TODO could check v
    set_raw_pixel(x,y,v);
    return 0;
}

/*
r,g1,b,g2=rawop.get_pixels_rgbg(x,y)
returns: values of the CFA quad containing x,y or nil if out of bounds
x and y are truncated to the nearest even value.

An error is generated if the this function is called outside the raw hook, or in a shooting
mode for which raw data is not available.
*/
static int rawop_get_pixels_rgbg(lua_State *L) {
    if(!raw_buffer_valid) {
        return luaL_error(L,"raw data not available");
    }
    unsigned int x=luaL_checknumber(L,1);
    unsigned int y=luaL_checknumber(L,2);

    x &= 0xFFFFFFFE;
    y &= 0xFFFFFFFE;

    if(x >= (unsigned)camera_sensor.raw_rowpix || y >= (unsigned)camera_sensor.raw_rows) {
        return 0;
    }
    lua_pushnumber(L,get_raw_pixel(x+cfa_offsets[CFA_R][0],y+cfa_offsets[CFA_R][1]));
    lua_pushnumber(L,get_raw_pixel(x+cfa_offsets[CFA_G1][0],y+cfa_offsets[CFA_G1][1]));
    lua_pushnumber(L,get_raw_pixel(x+cfa_offsets[CFA_B][0],y+cfa_offsets[CFA_B][1]));
    lua_pushnumber(L,get_raw_pixel(x+cfa_offsets[CFA_G2][0],y+cfa_offsets[CFA_G2][1]));
    return 4;
}

/*
rawop.set_pixels_rgbg(x,y,r,g1,b[,g2])
sets the values of the CFA quad containing x,y
if g2 is not specified, it is set to g1
x and y are truncated to the nearest even value.

An error is generated if the this function is called outside the raw hook, or in a shooting
mode for which raw data is not available.
*/
static int rawop_set_pixels_rgbg(lua_State *L) {
    if(!raw_buffer_valid) {
        return luaL_error(L,"raw data not available");
    }
    unsigned int x=luaL_checknumber(L,1);
    unsigned int y=luaL_checknumber(L,2);
    unsigned short r=luaL_checknumber(L,3);
    unsigned short g1=luaL_checknumber(L,4);
    unsigned short b=luaL_checknumber(L,5);
    unsigned short g2=luaL_optnumber(L,6,g1);

    x &= 0xFFFFFFFE;
    y &= 0xFFFFFFFE;

    if(x >= (unsigned)camera_sensor.raw_rowpix - 1 || y >= (unsigned)camera_sensor.raw_rows - 1) {
        return 0;
    }
    set_raw_pixel(x+cfa_offsets[CFA_R][0],y+cfa_offsets[CFA_R][1],r);
    set_raw_pixel(x+cfa_offsets[CFA_G1][0],y+cfa_offsets[CFA_G1][1],g1);
    set_raw_pixel(x+cfa_offsets[CFA_B][0],y+cfa_offsets[CFA_B][1],b);
    set_raw_pixel(x+cfa_offsets[CFA_G2][0],y+cfa_offsets[CFA_G2][1],g2);
    return 0;
}

/*
rawop.fill_rect(x,y,width,height,val[,xstep[,ystep]])
sets every step-th pixel of the specified rectangle to the specified value
width and height out of bounds are clipped
xstep defaults to 1, ystep defaults to xstep
step 2 can be used with cfa offsets to fill RGB

An error is generated if the this function is called outside the raw hook, or in a shooting
mode for which raw data is not available.
*/
static int rawop_fill_rect(lua_State *L) {
    if(!raw_buffer_valid) {
        return luaL_error(L,"raw data not available");
    }
    unsigned int xstart=luaL_checknumber(L,1);
    unsigned int ystart=luaL_checknumber(L,2);
    unsigned int width=luaL_checknumber(L,3);
    unsigned int height=luaL_checknumber(L,4);
    unsigned short val=luaL_checknumber(L,5);
    unsigned int xstep=luaL_optnumber(L,6,1);
    unsigned int ystep=luaL_optnumber(L,7,xstep);
    unsigned int xmax = xstart + width;
    unsigned int ymax = ystart + height;
    if(xstart >= (unsigned)camera_sensor.raw_rowpix || ystart >= (unsigned)camera_sensor.raw_rows) {
        return 0;
    }
    if(xmax > (unsigned)camera_sensor.raw_rowpix) {
        xmax = (unsigned)camera_sensor.raw_rowpix; 
    }
    if(ymax > (unsigned)camera_sensor.raw_rows) {
        ymax = (unsigned)camera_sensor.raw_rows;
    }
    unsigned x,y;
    for(y=ystart; y<ymax; y+=ystep) {
        for(x=xstart; x<xmax; x+=xstep) {
            set_raw_pixel(x,y,val);
        }
    }
    return 0;
}

/*
mean_raw_val=rawop.meter(x,y,x_count,y_count,x_step,y_step)
returns: average values of count pixels in x and y, sampled at step size step,
or nil if the range is invalid or the total number of pixels could result in overflow

An error is generated if the this function is called outside the raw hook, or in a shooting
mode for which raw data is not available.

To prevent overflow, the total number of pixels must less unsigned_max / white_level.
Limits are roughly 
10 bpp = 4 Mpix
12 bpp = 1 Mpix
14 bpp = 256 Kpix
To meter larger numbers of pixels, use multiple calls and average the results

To meter R G B separately, use multiple meter calls with the appropriate CFA offset and even steps
To ensure all CFA colors are included in a single call, use odd steps
*/
static int rawop_meter(lua_State *L) {
    if(!raw_buffer_valid) {
        return luaL_error(L,"raw data not available");
    }
    unsigned x1=luaL_checknumber(L,1);
    unsigned y1=luaL_checknumber(L,2);
    unsigned x_count=luaL_checknumber(L,3);
    unsigned y_count=luaL_checknumber(L,4);
    unsigned x_step=luaL_checknumber(L,5);
    unsigned y_step=luaL_checknumber(L,6);

    // no pixels
    if(!x_count || !y_count) {
        return 0;
    }

    unsigned x_max = x1 + x_step * x_count;
    unsigned y_max = y1 + y_step * y_count;
    // x out of range
    if(x_max > (unsigned)camera_sensor.raw_rowpix) {
        return 0;
    }
    // y out of range
    if(y_max > (unsigned)camera_sensor.raw_rows) {
        return 0;
    }
    // overflow possible (int max)/total  < max value 
    if(x_count*y_count > (unsigned)0xFFFFFFFF >> camera_sensor.bits_per_pixel) {
        return 0;
    }
    unsigned t=0;
    unsigned x,y;
    for(y = y1; y < y_max; y += y_step) {
        for(x = x1; x < x_max; x += x_step) {
            t+=get_raw_pixel(x,y);
        }
    }
    lua_pushnumber(L,t/(x_count*y_count));
    return 1;
}

/*
raw value conversion functions
*/
/*
ev96=rawop.raw_to_ev(rawval[,scale])
convert a raw value (blacklevel+1 to whitelevel) into an APEX EV relative to neutral
if rawval is <= to blacklevel, it is clamped to blacklevel + 1.
values > whitelevel are converted normally

scale optionally scales the APEX value, default 96 to be compatible with camera
exposure parameters. Use 1000 for imath APEX values or 96000 for APEX96 with imath

NOTE on G1x, the result may change depending on ISO settings. The value is only
updated when the raw hook is entered for the shot so for maximum portability,
this function should only be called inside the raw hook.
*/
static int rawop_raw_to_ev(lua_State *L) {
    int v=luaL_checknumber(L,1);
    int scale=luaL_optnumber(L,2,96);
    // TODO not clear what we should return, minimum real value for now
    if( v <= camera_sensor.black_level) {
        v = camera_sensor.black_level+1;
    }
    int r=round_d2i((double)scale*(log2(v - camera_sensor.black_level) - log2_raw_neutral_count));
    lua_pushnumber(L,r);
    return 1;
}

/*
rawval=rawop.ev_to_raw(ev[,scale])
Convert an APEX EV (offset from raw_neutral) to a raw value. No range checking is done

scale optionally scales the APEX value, default 96 to be compatible with camera
exposure parameters. Use 1000 for imath or 96000 for APEX96 with imath

NOTE on G1x, the result may change depending on ISO settings. The value is only
updated when the raw hook is entered for the shot so for maximum portability,
this function should only be called inside the raw hook.
*/
static int rawop_ev_to_raw(lua_State *L) {
    int v=luaL_checknumber(L,1);
    int scale=luaL_optnumber(L,2,96);
    // TODO not clear if this should be clamped to valid raw ranges?
    lua_pushnumber(L,round_d2i(pow(2,(double)v/(double)scale+log2_raw_neutral_count)+camera_sensor.black_level));
    return 1;
}


/*
histogram functions and methods
*/
typedef struct {
    unsigned bits;
    unsigned entries;
    unsigned total_pixels;
    unsigned *data;
} rawop_histo_t;
/*
create new histogram object
histo=rawop.create_histogram()
*/
static int rawop_create_histogram(lua_State *L) {
    rawop_histo_t *h = (rawop_histo_t *)lua_newuserdata(L,sizeof(rawop_histo_t));
    if(!h) {
        return luaL_error(L,"failed to create userdata");
    }
    h->total_pixels = 0;
    h->data = NULL;
    luaL_getmetatable(L, RAWOP_HISTO_META);
    lua_setmetatable(L, -2);
    return 1;
}

/*
update histogram data using specified area of raw buffer
histo:update(top,left,width,height,xstep,ystep[,bits])
bits specifies the bit depth of histogram. defaults to camera bit depth
must be <= camera bit depth
amount of memory required for the histogram data is determined by bits

An error is generated if the this function is called outside the raw hook, or in a shooting
mode for which raw data is not available.
*/
static int rawop_histo_update(lua_State *L) {
    if(!raw_buffer_valid) {
        return luaL_error(L,"raw data not available");
    }
    rawop_histo_t *h = (rawop_histo_t *)luaL_checkudata(L,1,RAWOP_HISTO_META);

    unsigned xstart=luaL_checknumber(L,2);
    unsigned ystart=luaL_checknumber(L,3);
    unsigned width=luaL_checknumber(L,4);
    unsigned height=luaL_checknumber(L,5);
    unsigned xstep=luaL_checknumber(L,6);
    unsigned ystep=luaL_checknumber(L,7);
    int bits=luaL_optnumber(L,8,camera_sensor.bits_per_pixel);
    if(bits > camera_sensor.bits_per_pixel || bits < 1) {
        return luaL_error(L,"invalid bit depth");
    }
    unsigned shift = camera_sensor.bits_per_pixel - bits;
    unsigned entries = 1 << bits;

    h->total_pixels=0;
    if(xstart >= (unsigned)camera_sensor.raw_rowpix 
        || ystart >= (unsigned)camera_sensor.raw_rows
        || xstep == 0 || ystep == 0
        || width == 0 || height == 0) {
        luaL_error(L,"invalid update area");
        return 0;
    }
    unsigned xmax=xstart+width;
    unsigned ymax=ystart+height;
    if(xmax > (unsigned)camera_sensor.raw_rowpix) {
        xmax = (unsigned)camera_sensor.raw_rowpix; 
    }
    if(ymax > (unsigned)camera_sensor.raw_rows) {
        ymax = (unsigned)camera_sensor.raw_rows;
    }
    // total with clipping and rounding accounted for
    h->total_pixels=((1+(xmax - xstart - 1)/xstep))*((1+(ymax - ystart - 1)/ystep));

    // TODO shorts or ints based on total pixels
    if(h->entries != entries || !h->data) {
        free(h->data);
        h->data = malloc(entries*sizeof(unsigned));
        if(!h->data) {
            h->total_pixels=0;
            return luaL_error(L,"insufficient memory");
        }
    }
    h->entries = entries;
    h->bits = bits;
    memset(h->data,0,h->entries*sizeof(unsigned));

    unsigned x,y;
    if(shift) {
        for(y=ystart;y<ymax;y+=ystep) {
            for(x=xstart;x<xmax;x+=xstep) {
                h->data[get_raw_pixel(x,y)>>shift]++;
            }
        }
    } else {
        for(y=ystart;y<ymax;y+=ystep) {
            for(x=xstart;x<xmax;x+=xstep) {
                h->data[get_raw_pixel(x,y)]++;
            }
        }
    }
    return 0;
}

/*
frac=histo:range(min,max[,'count'|scale])
returns number of values in range, either as a fraction in parts per scale, or total count
*/
static int rawop_histo_range(lua_State *L) {
    rawop_histo_t *h = (rawop_histo_t *)luaL_checkudata(L,1,RAWOP_HISTO_META);
    if(!h->data) {
        return luaL_error(L,"no data");
    }
    unsigned minval=luaL_checknumber(L,2);
    unsigned maxval=luaL_checknumber(L,3);
    int scale=1000;
    if(lua_gettop(L) >= 4 && !lua_isnil(L,4)) {
        const char *s=lua_tostring(L,4);
        if(!s || strcmp(s,"count") != 0) {
            scale=lua_tonumber(L,4);
            // scale could be 0 from error passing 0, but neither is valid
            if(!scale) {
                return luaL_error(L,"invalid format");
            }
        } else {
            scale=0;
        }
    }
    if(maxval >= h->entries || minval > maxval) {
        return luaL_error(L,"invalid range");
    }
    // TODO error?
    if(!h->total_pixels) {
        return luaL_error(L,"no pixels");
//        lua_pushnumber(L,0);
//        return 1;
    }

    unsigned count=0;
    unsigned i;
    for(i=minval;i<=maxval;i++) {
        count+=h->data[i];
    }
    // TODO full raw buffer count*1000 could overflow 32 bit int
    // could check / work around using ints but probably not worth it
    if(scale) {
        lua_pushnumber(L,round_d2i((scale*(double)count)/(double)h->total_pixels));
    } else {
        lua_pushnumber(L,count);
    }
    return 1;
}

/*
total=histo:total_pixels()
returns: total number of pixels sampled
*/
static int rawop_histo_total_pixels(lua_State *L) {
    rawop_histo_t *h = (rawop_histo_t *)luaL_checkudata(L,1,RAWOP_HISTO_META);
    if(!h->data) {
        return luaL_error(L,"no data");
    }
    lua_pushnumber(L,h->total_pixels);
    return 1;
}

/*
bits=histo:bits()
returns bit depth of histogram
*/
static int rawop_histo_bits(lua_State *L) {
    rawop_histo_t *h = (rawop_histo_t *)luaL_checkudata(L,1,RAWOP_HISTO_META);
    if(!h->data) {
        return luaL_error(L,"no data");
    }
    lua_pushnumber(L,h->bits);
    return 1;
}

/*
histo:free()
free memory used by histogram data. histo:update must be called again to before any other functions
*/
static int rawop_histo_free(lua_State *L) {
    rawop_histo_t *h = (rawop_histo_t *)luaL_checkudata(L,1,RAWOP_HISTO_META);
    free(h->data);
    h->data=NULL;
    return 0;
}

static int rawop_histo_gc(lua_State *L) {
    rawop_histo_free(L);
    return 0;
}

static const luaL_Reg rawop_histo_meta_methods[] = {
  {"__gc", rawop_histo_gc},
  {NULL, NULL}
};

static const luaL_Reg rawop_histo_methods[] = {
  {"update", rawop_histo_update},
  {"range", rawop_histo_range},
  {"total_pixels", rawop_histo_total_pixels},
  {"bits", rawop_histo_bits},
  {"free", rawop_histo_free},
  {NULL, NULL}
};

static const luaL_Reg rawop_funcs[] = {
  // general raw characteristics
  {"get_cfa",           rawop_get_cfa},
  {"get_cfa_offsets",   rawop_get_cfa_offsets},
  {"get_bits_per_pixel",rawop_get_bits_per_pixel},
  {"get_raw_neutral",   rawop_get_raw_neutral},
  {"get_black_level",   rawop_get_black_level},
  {"get_white_level",   rawop_get_white_level},

  // buffer sizes
  {"get_raw_width",     rawop_get_raw_width},
  {"get_raw_height",    rawop_get_raw_height},
  {"get_active_left",   rawop_get_active_left},
  {"get_active_top",    rawop_get_active_top},
  {"get_active_width",  rawop_get_active_width},
  {"get_active_height", rawop_get_active_height},
  {"get_jpeg_left",     rawop_get_jpeg_left},
  {"get_jpeg_top",      rawop_get_jpeg_top},
  {"get_jpeg_width",    rawop_get_jpeg_width},
  {"get_jpeg_height",   rawop_get_jpeg_height},

  // raw buffer access
  {"get_pixel",         rawop_get_pixel},
  {"set_pixel",         rawop_set_pixel},
  {"get_pixels_rgbg",   rawop_get_pixels_rgbg},
  {"set_pixels_rgbg",   rawop_set_pixels_rgbg},
  {"fill_rect",         rawop_fill_rect},
  {"meter",             rawop_meter},

  // value conversion
  {"raw_to_ev",         rawop_raw_to_ev},
  {"ev_to_raw",         rawop_ev_to_raw},

  // histogram
  {"create_histogram",  rawop_create_histogram},
  {NULL, NULL}
};

// initialize raw params that may change between frames (currently neutral and related values)
// could update only if changed, but not needed
static void init_raw_params(void) {
    // empirical guestimate
    // average pixel value of a neutral subject shot with canon AE, as a fraction of usable dynamic range
    // found to be reasonably close on d10, elph130, a540, g1x and more.
    double raw_neutral_count = (double)(camera_sensor.white_level - camera_sensor.black_level)/(6.669);
    log2_raw_neutral_count = log2(raw_neutral_count);
    raw_neutral = round_d2i(raw_neutral_count) + camera_sensor.black_level;
}

// update values that need to be updated when hook becomes active
void rawop_update_hook_status(int active) {
    if(active) {
        raw_buffer_valid = is_raw_possible();
        init_raw_params();
    } else {
        raw_buffer_valid = 0;
    }
}

int luaopen_rawop(lua_State *L) {
    // initialize globals
    raw_buffer_valid = 0;

    int i;
    int g1=1;
    for(i=0; i<4; i++) {
        int c = (camera_sensor.cfa_pattern >> 8*i) & 0xFF;
        int ci=0;
        switch(c) {
            case 0:
                ci=0;
            break;
            case 1:
                if(g1) {
                    ci=1;
                    g1=0;
                } else {
                    ci=2;
                }
            break;
            case 2:
                ci=3;
            break;
        }
        cfa_offsets[ci][0] = i&1;
        cfa_offsets[ci][1] = (i&2)>>1;
    }

    // TODO - maybe this should only be done in update_hook_status, since any changeable values
    // will only be known at that point.
    init_raw_params();

    luaL_newmetatable(L,RAWOP_HISTO_META);
    luaL_register(L, NULL, rawop_histo_meta_methods);  
    /* use a table of methods for the __index method */
    lua_newtable(L);
    luaL_register(L, NULL, rawop_histo_methods);  
    lua_setfield(L,-2,"__index");
    lua_pop(L,1);

    /* global lib*/
    lua_newtable(L);
    luaL_register(L, "rawop", rawop_funcs);  
    return 1;
}
