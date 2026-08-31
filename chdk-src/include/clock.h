#ifndef CLOCK_H
#define CLOCK_H

// CHDK interface to firmware timer / clock functions

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

extern long get_tick_count();

extern int SetHPTimerAfterNow(int delay, int(*good_cb)(int, int), int(*bad_cb)(int, int), int param);
extern int CancelHPTimer(int handle);

/*
sets camera clock, including RTC
values are as they appear in camera UI, full year, month and day start at 1
does not change DST state, time set is time displayed
also updates tick_count_offset
no validation in CHDK
*/
extern void set_clock(int year, int month, int day, int hour, int minute, int second);

#endif
