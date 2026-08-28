#include "fft/world.h"
#include "psx/types.h"

/* When set, the screen fade helpers at 0x8012d418-0x8012d6c8 all bail out
 * immediately, leaving g_world_gfx_fade_intensity at 0x801ca760 untouched. */
void world_gfx_set_fade_disabled(u8 disabled) {
    g_world_gfx_fade_disabled = disabled;
}
