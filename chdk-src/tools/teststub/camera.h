// Host stub - see tools/bend_shot_selftest.c.
//
// core/bendx.c gates itself on CAM_BEND_EXPERIMENTAL, which normally arrives
// through camera.h from the port's platform_camera.h. There is no port on the
// host, so this supplies the two values the engine needs and nothing else.
#ifndef TESTSTUB_CAMERA_H
#define TESTSTUB_CAMERA_H
#define CAM_BEND_EXPERIMENTAL       1
#define CAM_SENSOR_BITS_PER_PIXEL   12
#endif
