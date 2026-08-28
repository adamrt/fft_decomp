#include "psx/types.h"

s32 world_coords_fit_in_byte(s32 x, s32 y) {
    if ((u32)x < 0xff) {
        return (u32)y < 0xff;
    }
    return 0;
}
