#include "psx/types.h"

void battle_gfx_set_primitive_brightness(u8* primitive, s32 brightness) {
    primitive[4] = brightness;
    primitive[5] = brightness;
    primitive[6] = brightness;
}
