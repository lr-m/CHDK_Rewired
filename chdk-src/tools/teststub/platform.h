// Host stub for tools/bend_shot_selftest.c - see that file.
//
// core/bend_shot.c is ordinary file I/O and string arithmetic, and both are
// worth testing off-camera: it runs inside the capture hook, on a trip, once
// per frame. This provides just enough of the CHDK environment to compile it
// on the host, with the file calls redirected into memory by the test.
#ifndef TESTSTUB_PLATFORM_H
#define TESTSTUB_PLATFORM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAM_SENSOR_BITS_PER_PIXEL 12

#define O_RDONLY  0
#define O_WRONLY  1
#define O_CREAT   0x100
#define O_TRUNC   0x200

int  stub_open(const char *name, int flags, int mode);
int  stub_read(int fd, void *buf, int n);
int  stub_write(int fd, const void *buf, int n);
int  stub_close(int fd);
int  stub_remove(const char *name);

#define open(a,b,c) stub_open((a),(b),(c))
#define read(a,b,c) stub_read((a),(b),(c))
#define write(a,b,c) stub_write((a),(b),(c))
#define close(a) stub_close((a))
#define remove(a) stub_remove((a))

#endif
