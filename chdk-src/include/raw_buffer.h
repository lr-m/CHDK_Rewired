#ifndef RAW_BUFFER_H
#define RAW_BUFFER_H

// CHDK Raw buffer interface

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

extern char *hook_raw_image_addr();
extern char *hook_alt_raw_image_addr();

// The JPEG encoder's working buffer, for the JPEG bus experimental profile -
// see docs/EXPERIMENTAL_EFFECTS.md. Read only, and only ever between shots.
//
// Returns 0 on a port that has not reversed it, which is the default: the
// address and the size have to come out of that firmware's own buffer table
// and there is no way to derive either. A port returning 0 gets a JPEG bus
// entry that does nothing, the same as any other unsupplied foreign bus.
extern char *hook_jpeg_buffer(unsigned *len);

#endif
