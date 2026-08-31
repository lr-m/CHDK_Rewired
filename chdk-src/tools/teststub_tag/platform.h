// Host stub for tools/bend_tag_selftest.c - see that file.
//
// core/bend_tag.c rewrites photographs. That is the one thing in this fork
// that can destroy something the user cannot get back, so it is tested on the
// host against real files rather than against an in-memory shim: the failure
// worth catching is "the copy is subtly wrong", and only real bytes on a real
// filesystem prove that it is not.
#ifndef TESTSTUB_TAG_PLATFORM_H
#define TESTSTUB_TAG_PLATFORM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define CAM_SENSOR_BITS_PER_PIXEL 10
#define CAM_BEND_EXPERIMENTAL 1

unsigned stub_tick(void);
#define get_tick_count() stub_tick()

#endif
