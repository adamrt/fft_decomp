#include "fft/battle_gfx.h"
#include "psx/libgte.h"

/* Starts a frame of sprite drawing: the GTE offset is reset to (0, 0) and the
 * primitive ring index reached last frame is recorded. */
void battle_gfx_init_render_frame(void) {
    SetGeomOffset(0, 0);
    /* The target loads the counter unsigned (lhu). */
    g_battle_gfx_previous_counter = g_battle_gfx_counter;
}
