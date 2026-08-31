/*
lfmathlib.c,v 1.0
double precision floating point based mathematics for CHDK Lua scripts

(c)2021 philmoz
License GPL 2.0

fmath library usage in Lua:

Create a new fmath double variable:
    x = fmath.new(n,d)
Set x to n / d.
E.G.
    x = fmath.new(45,10) - x = 4.5
    y = fmath.new(25,100) = y = 0.25
Parameter d is optional and defaults to 1. x = fmath.new(5) is equivalent to x = fmath.new(5,1)

Arithmetic - use standard operators.
E.G.
    z = x + y
    z = x - y
    z = x * y
    z = x / y
    z = -x      (unary minus)
    z = x ^ y   (power function)
    z = x % y   (modulus)
So long as either parameter is an fmath double the result will be an fmath double. The following are both valid:
    z = x + 1
    z = 1 + x

Comparison - as above use standard operators ==, ~=, <, <=, >, >=
Note: for comparison operators both sides must be fmath doubles. You cannot mix fmath double and integer values in comparisons.

Math functions:
    log, log2, log10, sqrt
Can be called via fmath library or object oriented syntax. The following are equivalent:
    y = fmath.log(x)	==	y = x:log()
    y = fmath.sqrt(x)	==	y = x:sqrt();

Conversion:
    int, ceil, floor, round, deg, rad
These can also be called via the fmath library or object oriented syntax as above.
The int, ceil, floor and round functions return integer values not fmath doubles.
The deg and rad functions convert between degrees and radians and return an fmath double value.

The frac function from imathlib is not implemented. The imathlib function is mathematically wrong for negative values.
You can get the equivalent of the imathlib frac function using:
    f = (x - x:int()) * 1000
The correct value would be:
    f = (x - x:floor()) * 1000

Trigonometry:
    sin, cos, tan, asin, acos, atan, pol, rec
The same algorithms as used by the imathlib library are used here.
Note: all functions operate on or return values in radians.
*/

#include "stdlib.h"
#include "math.h"

#define lfmathlib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#define fabs(x)     ((((x) < 0.0) ? -(x) : (x)))

// get function argument, if not valid fmath 'double' check for integer
static double arg(lua_State *L, int n) {
    double* v = (double*)lua_touserdata(L, n);
    if (v == 0) {
        int i = luaL_checknumber(L, n);
        return (double)i;
    }
    return *v;
}

// create a new fmath 'double' with given value
static double* newval(lua_State *L, double v) {
    double* r = (double*)lua_newuserdata(L, sizeof(double));
    luaL_getmetatable(L, "fmathmeta");
    lua_setmetatable(L, -2);
     *r = v;
    return r;
}

static int fmath_new (lua_State *L) {
    int n = luaL_checknumber(L, 1);
    int d = luaL_optnumber(L, 2, 1);
    if (d == 0) luaL_error(L, "divide by 0");
    newval(L, (double)n / (double)d);
    return 1;
}

// Due to lack of support for %f format string on some cameras the toStr function uses a simple integer conversion for formatting
// The optional 2nd parameter specifies the number of decimal digits to output - default is 6.
static int fmath_toStr (lua_State *L) {
    char buf[100];
    // value
    double a = arg(L, 1);
    // precision
    int p = luaL_optnumber(L, 2, 6);
    if (p < 0) p = 0;
    if (p > 9) p = 9;
    // decimal scaling / rounding factor
    double m = pow(10.0, p);
    // sign
    const char* s = (a < 0) ? "-" : "";
    // round absolute value
    a = fabs(a) + (0.5 / m);
    // whole part
    unsigned int w = (int)a;
    if (p > 0) {
        // decimal part
        unsigned int d = (int)((a - w) * m);
        // format string
        char fmt[20];
        sprintf(fmt, "%s%%u.%%0%du", s, p);
        // value string
        sprintf(buf, fmt, w, d);
    } else {
        // value string
        sprintf(buf, "%s%d", s, w);
    }
    lua_pushstring(L, buf);
    return 1;
}

enum {
    FN_MUL = 0,
    FN_DIV,
    FN_ADD,
    FN_SUB,
    FN_POW,
    FN_MOD,
    FN_LOG,
    FN_LOG2,
    FN_LOG10,
    FN_SQRT,
    FN_NEG,
    FN_DEG,
    FN_RAD,
    FN_EQ,
    FN_LT,
    FN_LE,
    FN_INT,
    FN_CEIL,
    FN_FLOOR,
    FN_ROUND,
    FN_SIN,
    FN_COS,
    FN_TAN,
    FN_ASIN,
    FN_ACOS,
    FN_ATAN,
};

enum {
    ROTATE = 0,
    VECTOR
};

/*
    Cordic trig functions.
    Copied from cordic_math.c and converted to use double data type.
 */
 
#define N   17
#define M   9

static double atan_tab[M] = {
    0.78539816339745, 0.46364760900081, 0.24497866312686, 0.12435499454676,
    0.06241880999596, 0.03123983343027, 0.01562372862048, 0.00781234106010,
    0.00390623013197
};
#define INV_GAIN_CIRCLE     0.60725293501477

// CORDIC main routine
static void cordic(int f, double *x, double *y, double *z) {
    double *patan_tab = atan_tab;
    double xstep, ystep, zstep = 1;
    double div = 1;
    int i;
    for (i = 0; i < N; i += 1) {
        xstep = *x / div;
        ystep = *y / div;
        if (i < M) {
            zstep = *patan_tab;
            ++patan_tab;
        } else zstep = zstep / 2.0;
        int f1 = (f) ? *y >= 0 : *z < 0;
        *x = (f1) ? *x + ystep : *x - ystep;
        *y = (f1) ? *y - xstep : *y + xstep;
        *z = (f1) ? *z + zstep : *z - zstep;
        div = div * 2.0;
    }
}

#ifndef HOST_LUA
//chdk: fmod not on math.h

#define trunc(x)    ((double)((int)(x)))

static double fmod(double x, double y) {
    return x - trunc(x / y) * y;
}
#endif

static double cathetus(double x) {
    return sqrt((1 + x) * (1 - x));
}

// base CIRCULAR mode, ROTATE
static void sincosCordic(double phi, double *sinphi, double *cosphi) {
    // convert to quadrant 1
    phi = fmod(fmod(phi, M_PI * 2.0) + (M_PI * 2.0), M_PI * 2.0);
    int q = (int)(phi / (M_PI / 2.0)) + 1;
    switch(q) {
        case 2: phi = M_PI - phi; break;
        case 3: phi = phi - M_PI; break;
        case 4: phi = M_PI * 2.0 - phi; break;
    }

    double x = INV_GAIN_CIRCLE, y = 0, z = phi;
    cordic(ROTATE, &x, &y, &z);

    // convert to original quadrant
    if ((q == 2) || (q == 3)) { x = -x; }
    if ((q == 3) || (q == 4)) { y = -y; }

    *sinphi = y;
    *cosphi = x;
}

// base CIRCULAR mode, VECTOR
static void atanhypCordic(double x, double y, double *phi, double *hyp) {
    if (x == 0 && y == 0) {
        // error input vales
        *phi = 0;
        *hyp = 0;
        return;
    }

    // convert to quadrant 1
    int fy = (y >= 0);
    int q = 1;
    if (x < 0) {
        if (fy)
            q = 2;
        else
            q = 3;
    } else if (!fy) {
        q = 4;
    }
    x = fabs(x);
    y = fabs(y);

    double z = 0;
    cordic(VECTOR, &x, &y, &z);

    // convert to original quadrant
    switch(q) {
        case 2: z = M_PI - z; break;
        case 3: z = z - M_PI; break;
        case 4: z = -z; break;
    }

    *phi = z;
    *hyp = x;
}

/*
    A note on the implementation below.
    Floating point math operations require long calls to core CHDK functions.
    GCC currently does not merge the long call offsets to the same core function across different functions here.
    So two local functions that perform floating point division will end up with two copies of the core division function offset.
    The implementation here with switch statements is a bit messy; but helps reduce the overhead and compiled size.
*/

static int twoargfn (lua_State *L, int fn) {
    double a = arg(L, 1);
    double b = arg(L, 2);
    double r = 0;
    switch(fn) {
    case FN_MUL:
        r = a * b;
        break;
    case FN_DIV:
        r = a / b;
        break;
    case FN_ADD:
        r = a + b;
        break;
    case FN_SUB:
        r = a - b;
        break;
    case FN_POW:
        r = pow(a, b);
        break;
    case FN_MOD:
        r = fmod(a, b);
        break;
    default:
        break;
    }
    newval(L, r);
    return 1;
}

static int oneargfn (lua_State *L, int fn) {
    double a = arg(L, 1);
    double r = 0;
    switch(fn) {
    case FN_LOG:
        r = log(a);
        break;
    case FN_LOG2:
        r = log2(a);
        break;
    case FN_LOG10:
        r = log10(a);
        break;
    case FN_SQRT:
        r = sqrt(a);
        break;
    case FN_NEG:
        r = -a;
        break;
    case FN_DEG:
        r =  a * 180.0 / M_PI;
        break;
    case FN_RAD:
        r =  a * M_PI / 180.0;
        break;
    default:
        break;
    }
    newval(L, r);
    return 1;
}

static int boolfn (lua_State *L, int fn) {
    double a = arg(L, 1);
    double b = arg(L, 2);
    int r = 0;
    switch(fn) {
    case FN_EQ:
        r = (a == b);
        break;
    case FN_LT:
        r = (a < b);
        break;
    case FN_LE:
        r = (a <= b);
        break;
    default:
        break;
    }
    lua_pushboolean(L, r);
    return 1;
}

static int intfn (lua_State *L, int fn) {
    double a = arg(L, 1);
    double frac = a - (int)a;
    int r = 0;
    switch(fn) {
    case FN_INT:
        r = (int)a;
        break;
    case FN_CEIL:
        if ((a > 0.0) && (frac != 0.0))
            r = (int)a + 1;
        else
            r = (int)a;
        break;
    case FN_FLOOR:
        if ((a < 0.0) && (frac != 0.0))
            r = (int)a - 1;
        else
            r = (int)a;
        break;
    case FN_ROUND:
        if (a < 0.0)
            r = (int)(a - 0.5);
        else
            r = (int)(a + 0.5);
        break;
    default:
        break;
    }
    lua_pushnumber(L, r);
    return 1;
}

static int trigfn(lua_State *L, int fn) {
    double a = arg(L, 1);
    double _sin, _cos;
    double r = 0;
    sincosCordic(a, &_sin, &_cos);
    switch(fn) {
    case FN_SIN:
        r = _sin;
        break;
    case FN_COS:
        r = _cos;
        break;
    case FN_TAN:
        r = _sin / _cos;
        break;
    default:
        break;
    }
    newval(L, r);
    return 1;
}

static int atrigfn(lua_State *L, int fn) {
    double x = arg(L, 1);
    double y = 0;
    switch(fn) {
    case FN_ASIN:
        y = x;
        x = cathetus(x);
        break;
    case FN_ACOS:
        y = cathetus(x);
        break;
    case FN_ATAN:
        y = x;
        x = 1.0;
        break;
    default:
        break;
    }
    double phi, hyp;
    atanhypCordic(x, y, &phi, &hyp);
    newval(L, phi);
    return 1;
}

static int fmath_rec(lua_State *L) {
    double r = arg(L, 1);
    double theta = arg(L, 2);
    double _sin, _cos;
    sincosCordic(theta, &_sin, &_cos);
    newval(L, r * _cos);
    newval(L, r * _sin);
    return 2;
}

static int fmath_pol (lua_State *L) {
    double px = arg(L, 1);
    double py = arg(L, 2);
    double phi, hyp;
    atanhypCordic(px, py, &phi, &hyp);
    newval(L, hyp * INV_GAIN_CIRCLE);
    newval(L, phi);
    return 2;
}

static int fmath_mul (lua_State *L) { return twoargfn(L, FN_MUL); }
static int fmath_div (lua_State *L) { return twoargfn(L, FN_DIV); }
static int fmath_add (lua_State *L) { return twoargfn(L, FN_ADD); }
static int fmath_sub (lua_State *L) { return twoargfn(L, FN_SUB); }
static int fmath_pow (lua_State *L) { return twoargfn(L, FN_POW); }
static int fmath_mod (lua_State *L) { return twoargfn(L, FN_MOD); }

static int fmath_eq (lua_State *L) { return boolfn(L, FN_EQ); }
static int fmath_lt (lua_State *L) { return boolfn(L, FN_LT); }
static int fmath_le (lua_State *L) { return boolfn(L, FN_LE); }

static int fmath_neg (lua_State *L) { return oneargfn(L, FN_NEG); }
static int fmath_log (lua_State *L) { return oneargfn(L, FN_LOG); }
static int fmath_log2 (lua_State *L) { return oneargfn(L, FN_LOG2); }
static int fmath_log10 (lua_State *L) { return oneargfn(L, FN_LOG10); }
static int fmath_sqrt (lua_State *L) { return oneargfn(L, FN_SQRT); }
static int fmath_deg (lua_State *L) { return oneargfn(L, FN_DEG); }
static int fmath_rad (lua_State *L) { return oneargfn(L, FN_RAD); }

static int fmath_int (lua_State *L) { return intfn(L, FN_INT); }
static int fmath_ceil (lua_State *L) { return intfn(L, FN_CEIL); }
static int fmath_floor (lua_State *L) { return intfn(L, FN_FLOOR); }
static int fmath_round (lua_State *L) { return intfn(L, FN_ROUND); }

static int fmath_sin (lua_State *L) { return trigfn(L, FN_SIN); }
static int fmath_cos (lua_State *L) { return trigfn(L, FN_COS); }
static int fmath_tan (lua_State *L) { return trigfn(L, FN_TAN); }

static int fmath_asin (lua_State *L) { return atrigfn(L, FN_ASIN); }
static int fmath_acos (lua_State *L) { return atrigfn(L, FN_ACOS); }
static int fmath_atan (lua_State *L) { return atrigfn(L, FN_ATAN); }

static const luaL_Reg fmathlib_m[] = {
    {"__add",       fmath_add},
    {"__sub",       fmath_sub},
    {"__mul",       fmath_mul},
    {"__div",       fmath_div},
    {"__pow",       fmath_pow},
    {"__mod",       fmath_mod},
    {"__unm",       fmath_neg},
    {"__eq",        fmath_eq},
    {"__le",        fmath_le},
    {"__lt",        fmath_lt},
// If any new entries are added above then adjust the offset in the luaL_register call below
    {"new",         fmath_new},
    {"tostr",       fmath_toStr},
    {"log",         fmath_log},
    {"log2",        fmath_log2},
    {"log10",       fmath_log10},
    {"sqrt",        fmath_sqrt},
    {"int",         fmath_int},
    {"ceil",        fmath_ceil},
    {"floor",       fmath_floor},
    {"round",       fmath_round},
    {"deg",         fmath_deg},
    {"rad",         fmath_rad},
    {"sin",         fmath_sin},
    {"cos",         fmath_cos},
    {"tan",         fmath_tan},
    {"asin",        fmath_asin},
    {"acos",        fmath_acos},
    {"atan",        fmath_atan},
    {"pol",         fmath_pol},
    {"rec",         fmath_rec},
    {NULL, NULL}
};

/*
** Open fmath library
*/
LUALIB_API int luaopen_fmath (lua_State *L) {
    luaL_newmetatable(L, "fmathmeta");
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);   /* pushes the metatable */
    lua_settable(L, -3);    /* metatable.__index = metatable */
    luaL_register(L, NULL, fmathlib_m);

    luaL_register(L, LUA_FMATHLIBNAME, &fmathlib_m[7]);     // adjust offset if table is changed

    newval(L, M_PI * 2.0);
    lua_setfield(L, -2, "pi2");
    newval(L, M_PI);
    lua_setfield(L, -2, "pi");
    newval(L, M_PI / 2.0);
    lua_setfield(L, -2, "pi_2");

    return 1;
}
