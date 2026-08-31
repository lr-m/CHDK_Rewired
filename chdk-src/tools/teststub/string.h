// Host stub - see tools/bend_shot_selftest.c. Shadows the CHDK header of this
// name so core/bend_shot.c compiles on the host, then hands straight through
// to the real system one.
#ifndef TESTSTUB_string_H
#define TESTSTUB_string_H
#include_next <string.h>
#endif
