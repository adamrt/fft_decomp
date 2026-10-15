#include "fft/gfx.h"
#include "psx/libgpu.h"

/* Set the color in a GPU primitive's common header to one gray level. */
void gfx_set_primitive_brightness(P_TAG* primitive, s32 brightness) {
    setRGB0(primitive, brightness, brightness, brightness);
}
