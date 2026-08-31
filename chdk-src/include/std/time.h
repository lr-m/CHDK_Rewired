#ifndef TIME_H
#define TIME_H

// CHDK time

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

struct tm {
    int tm_sec;	/* seconds after the minute	- [0, 59] */
    int tm_min;	/* minutes after the hour	- [0, 59] */
    int tm_hour;	/* hours after midnight		- [0, 23] */
    int tm_mday;	/* day of the month		- [1, 31] */
    int tm_mon;	/* months since January		- [0, 11] */
    int tm_year;	/* years since 1900	*/
    int tm_wday;	/* days since Sunday		- [0, 6] */
    int tm_yday;	/* days since January 1		- [0, 365] */
    int tm_isdst;	/* Daylight Saving Time flag */
};

typedef unsigned long time_t;

extern struct tm * localtime(const time_t *_tod);
extern struct tm * get_localtime();

struct utimbuf {
    unsigned long actime;       /* set the access time */
    unsigned long modtime;      /* set the modification time */
};

extern int utime(const char *file, struct utimbuf *newTimes);
extern unsigned long time(unsigned long *timer);
extern long strftime(char *s, unsigned long maxsize, const char *format, const struct tm *timp);
extern time_t mktime(struct tm *timp);

//---------------------------------------------------------------

#endif
