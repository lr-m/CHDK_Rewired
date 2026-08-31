#ifndef GPS_MATH_H
#define GPS_MATH_H

// Additional math functions for GPS
// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

extern int fac(int n);
extern double arctan(double x, int n);
extern double arctan2(double y, double x);
extern double Round(double Zahl, int Stellen);

extern double ceil (double phi);
extern double floor (double phi);

/*
**  formatDouble:
**
**  result
**      Character buffer for the result
**      it is a pointer to this array returned
**      NULL -> private array is used
**
**  value
**      Value to be Displayed
**
**  length
**      Complete length of the formatted value
**
**  fracDigits
**      Number of decimal places
**
*/

typedef char t_format_result [40];

extern const char* formatDouble (t_format_result result, double value, unsigned length, unsigned fracDigits);

/*
   Number of "nodes".
   If not defined, the number and a pointer must be passed to a double array of Gr ??
   at initialization of the control block
 */


/*
   Control block for the calculation
 */

typedef struct {
    double x;
    double y;
} t_regression_xy;

typedef struct {
    /* buffer */
    int size;
    t_regression_xy* values;
    /* storage */
    int index;
    /* sums */
    double n;
    double sx;
    double sy;
    double sxy;
    double sxx;
    double last_x;
    /* result */
    int valid;
    double s;
    double t;
} t_regression;

extern void   regressionInit (t_regression *rcb, int size, t_regression_xy buffer[]);
extern void   regressionReset (t_regression *rcb);
extern void   regressionAdd (t_regression *rcb, double x, double y);
extern double regressionActual (t_regression *rcb);
extern double regressionForecast (t_regression *rcb, double x);
extern double regressionReverse (t_regression *rcb, double y);
extern double regressionChange (t_regression *rcb);
extern double regressionQuality (t_regression *rcb);
extern double sin (double phi);
extern double cos (double phi);

#endif
