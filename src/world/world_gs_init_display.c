#include "fft/world.h"
#include "psx/types.h"

/* libgs graphics bring-up: display mode, geometry engine, screen state and
 * the initial draw-buffer clip. */
void world_gs_init_display(u16 x, u16 y, u16 intmode, u16 dither, u16 vram) {
    world_gs_initgraph(x, y, intmode, dither, vram);
    world_gs_gte_init();
    g_world_gs_active_buffer = 0;
    world_gs_reset_screen_state(x, y);
    world_gs_setdrawbuffclip();
    world_gs_setdrawbuffoffset();
}
