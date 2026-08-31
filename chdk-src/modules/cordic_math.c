/*
CORDIC LIBRARY
integer based trigonometric mathematics
all values scaled by 100000 (CORDIC_SCALE)

based on http://www.andreadrian.de/c-workshop/index.html

(c)2012/2013 rudi from CHDK[-DE] forum
License GPL 2.0
*/

#include "stdlib.h"
#include "luaconf.h"
#include "math.h"
#include "cordic_math.h"

fixed atan_tab[][M] = {
    {0x1921f, 0xed63, 0x7d6d, 0x3fab, 0x1ff5, 0xffe, 0x7ff, 0x3ff, 0x1ff},
    {0x5a0000, 0x35214e, 0x1c128e, 0xe4002, 0x72715, 0x3946f, 0x1ca54, 0xe52d, 0x7297}
};
fixed INV_GAIN_CIRCLE[] = {0x136e9, 0x136e8};   // {0.60725, 0.60724}
fixed FULL_CIRCLE[]     = {0xc90fd, 0x2d00000}; // {6.28318, 360.00000}
fixed HALF_CIRCLE[]     = {0x6487e, 0x1680000}; // {3.14159, 180.00000}
fixed QUART_CIRCLE[]    = {0x3243f, 0xb40000};  // {1.57079, 90.00000}
fixed ATAN_LIMIT        = 0x36f602be;           // 7035.00536
fixed FIXED_MAX         = 16383.99999;

// CORDIC main routine
static void cordic(tangle t, fcordic f, fixed *x, fixed *y, fixed *z) {
    long *patan_tab = atan_tab[t];
    long xstep, ystep, zstep = 1;
    int i;
    for (i = 0; i < N; ++i) {
        xstep = *x >> i;
        ystep = *y >> i;
        if (i < M) {
            zstep = *patan_tab;
            ++patan_tab;
        } else zstep >>= 1;
        int f1 = (f) ? *y >= 0 : *z < 0;
        *x = (f1) ? *x + ystep : *x - ystep;
        *y = (f1) ? *y - xstep : *y + xstep;
        *z = (f1) ? *z + zstep : *z - zstep;
    }
}

// abs(INT_MIN) doesn't work
static int cordic_abs(int a) {
    if (a < -INT_MAX) a = -INT_MAX;
    return abs(a);
}

static int cordic_sign(int a) {
    return (a < 0)? -1 : 1;
}

// a x b / c
LUALIB_API fixed muldivScaled(fixed a, fixed b, fixed c) {
    double res;
    if (c != 0) {
        int sign = cordic_sign(a) * cordic_sign(b) / cordic_sign(c);
        res = FLOAT(cordic_abs(a)) * FLOAT(cordic_abs(b)) / FLOAT(cordic_abs(c));
        if (res > INT_MAX) {
            // error overflow
            res = INT_MAX;
        } else { res = sign * res; }
    } else {
        // error div 0
        res = 0;
    }
    return FIXED(res);
}

static fixed mulScaled(fixed a, fixed b) {
    return muldivScaled(a, b, CORDIC_SCALE);
}

static fixed divScaled(fixed a, fixed b) {
    return muldivScaled(CORDIC_SCALE, a, b);
}

static fixed cathetus(fixed x) {
    return FIXED(sqrt((1 + FLOAT(x)) * (1 - FLOAT(x))));
}

// base CIRCULAR mode, ROTATE
static void sincosCordic(tangle t, fixed phi, fixed *sinphi, fixed *cosphi) {
    // convert to quadrant 1
    phi = (phi % FULL_CIRCLE[t] + FULL_CIRCLE[t]) % FULL_CIRCLE[t];
    int q = phi / QUART_CIRCLE[t] + 1;
    switch(q) {
        case 2: phi = HALF_CIRCLE[t] - phi; break;
        case 3: phi = phi - HALF_CIRCLE[t]; break;
        case 4: phi = FULL_CIRCLE[t] - phi; break;
    }
    fixed x = INV_GAIN_CIRCLE[t], y = 0, z = phi;
    cordic(t, ROTATE, &x, &y, &z);
    // convert to original quadrant
    if ((q == 2) || (q == 3)) { x = -x; }
    if ((q == 3) || (q == 4)) { y = -y; }
    *sinphi = y;
    *cosphi = x;
}

// base CIRCULAR mode, VECTOR
static void atanhypCordic(tangle t, fixed px, fixed py, fixed *phi, fixed *hyp) {
    // convert to quadrant 1
    int fy = (py >= 0);
    int q = 1;
    if (px < 0) {
        if (fy)
            q = 2;
        else
            q = 3;
    } else if (!fy) {
        q = 4;
    }
    px = cordic_abs(px);
    py = cordic_abs(py);

    int f = 0;
    while (px > ATAN_LIMIT || py > ATAN_LIMIT) {
        px /= 2;
        py /= 2;
        f = 1;
    }
    if (px == 0 && py == 0) {
        // error input vales
        *phi = 0;
        *hyp = 0;
    } else {
        fixed x = px, y = py, z = 0;
        cordic(t, VECTOR, &x, &y, &z);
        // convert to original quadrant
        switch(q) {
            case 2: z = HALF_CIRCLE[t] - z; break;
            case 3: z = z - HALF_CIRCLE[t]; break;
            case 4: z = -z; break;
        }
        *phi = z;
        *hyp = (f)? 0 : x;
    }
}

// functions in CIRCULAR mode, ROTATE
static fixed sinCordic(tangle t, fixed phi) {
    fixed _sin, _cos;
    sincosCordic(t, phi, &_sin, &_cos);
    return _sin;
}

static fixed cosCordic(tangle t, fixed phi) {
    fixed _sin, _cos;
    sincosCordic(t, phi, &_sin, &_cos);
    return _cos;
}

static fixed tanCordic(tangle t, fixed phi) {
    fixed _sin, _cos;
    sincosCordic(t, phi, &_sin, &_cos);
    return divScaled(_sin, _cos);
}

static void recCordic(tangle t, fixed r, fixed theta, fixed *px, fixed *py) {
    fixed _sin, _cos;
    sincosCordic(t, theta, &_sin, &_cos);
    *px = mulScaled(r, _cos);
    *py = mulScaled(r, _sin);
}

// functions in CIRCULAR mode, VECTOR
static fixed asinCordic(tangle t, fixed x) {
    fixed phi, hyp;
    fixed _cos = cathetus(x);
    atanhypCordic(t, _cos, x, &phi, &hyp);
    return phi;
}

static fixed acosCordic(tangle t, fixed x) {
    fixed phi, hyp;
    fixed _sin = cathetus(x);
    atanhypCordic(t, x, _sin, &phi, &hyp);
    return phi;
}

static fixed atanCordic(tangle t, fixed x) {
    fixed phi, hyp;
    atanhypCordic(t, CORDIC_SCALE, x, &phi, &hyp);
    return phi;
}

static void polCordic(tangle t, fixed px, fixed py, fixed *r, fixed *theta) {
    fixed phi, hyp;
    atanhypCordic(t, px, py, &phi, &hyp);
    *theta = phi;
    *r = mulScaled(hyp, INV_GAIN_CIRCLE[t]);
}

// extern functions
// DEG
LUALIB_API fixed sind(fixed phi) {
    return sinCordic(DEG, phi);
}

LUALIB_API fixed cosd(fixed phi) {
    return cosCordic(DEG, phi);
}

LUALIB_API fixed tand(fixed phi) {
    return tanCordic(DEG, phi);
}

LUALIB_API void recd(fixed r, fixed theta, fixed *px, fixed *py) {
    recCordic(DEG, r, theta, px, py);
}

LUALIB_API fixed asind(fixed x) {
    return asinCordic(DEG, x);
}

LUALIB_API fixed acosd(fixed x) {
    return acosCordic(DEG, x);
}

LUALIB_API fixed atand(fixed x) {
    return atanCordic(DEG, x);
}

LUALIB_API void pold(fixed px, fixed py, fixed *r, fixed *theta) {
    polCordic(DEG, px, py, r, theta);
}

// RAD
LUALIB_API fixed sinr(fixed phi) {
    return sinCordic(RAD, phi);
}

LUALIB_API fixed cosr(fixed phi) {
    return cosCordic(RAD, phi);
}

LUALIB_API fixed tanr(fixed phi) {
    return tanCordic(RAD, phi);
}

LUALIB_API void recr(fixed r, fixed theta, fixed *px, fixed *py) {
    recCordic(RAD, r, theta, px, py);
}

LUALIB_API fixed asinr(fixed x) {
    return asinCordic(RAD, x);
}

LUALIB_API fixed acosr(fixed x) {
    return acosCordic(RAD, x);
}

LUALIB_API fixed atanr(fixed x) {
    return atanCordic(RAD, x);
}

LUALIB_API void polr(fixed px, fixed py, fixed *r, fixed *theta) {
    polCordic(RAD, px, py, r, theta);
}

//additional math functions
LUALIB_API fixed fint(fixed a) {
    return cordic_sign(a) * (cordic_abs(a) & CORDIC_INTEGER);
}

LUALIB_API fixed fceil(fixed a) {
    fixed int_a = fint(a);
    return (a < 0 || a == int_a)? int_a: int_a + CORDIC_SCALE;
}

LUALIB_API fixed ffloor(fixed a) {
    fixed int_a = fint(a);
    return (a > 0 || a == int_a)? int_a: int_a - CORDIC_SCALE;
}

LUALIB_API fixed fround(fixed a) {
    return ffloor(a + CORDIC_SCALE / 2);
}

// convert functions
LUALIB_API fixed floatToFixed(double a) {
    int sign = 1;
    if (a < 0) {
        sign = -1;
        a = -a;
    }
    if (a > FIXED_MAX) {
        // error fixed overflow
        a = FIXED_MAX;
    }
    return a * CORDIC_SCALE * sign;
}

LUALIB_API fixed intToFixed(int4b a, int round) {
    double res = cordic_abs(a);
    if (round) res = res + 0.5;
    res = res * CORDIC_SCALE / INT_SCALE;
    if (res > INT_MAX) {
        // error fixed overflow
        res = INT_MAX;
    }
    return cordic_sign(a) * res;
}

LUALIB_API int4b fixedToInt(fixed a, int round) {
    double res = cordic_abs(a);
    if (res > INT_MAX) {
        // error int4b overflow
        res = INT_MAX;
    }
    res = res * INT_SCALE / CORDIC_SCALE;
    if (round) res = res + 0.5;
    return cordic_sign(a) * res;
}
