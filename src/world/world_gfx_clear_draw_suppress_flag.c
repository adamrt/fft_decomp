#include "fft/world.h"
#include "psx/types.h"

/* Paired with world_gfx_set_draw_suppress_flag (0x800e8a34).  While the flag is non-zero the world
   display thread at 0x800e8ab0 skips its four primitive submissions. */
void world_gfx_clear_draw_suppress_flag(void) {
    g_world_gfx_draw_suppress_flag = 0;
}
