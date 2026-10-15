#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/gte_inline.h"
#include "psx/gte_inline_split.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Starts a frame of sprite drawing: the GTE offset is reset to (0, 0) and the
 * primitive ring index reached last frame is recorded. */
void battle_gfx_init_render_frame(void) {
    SetGeomOffset(0, 0);
    /* The target loads the counter unsigned (lhu). */
    g_battle_gfx_previous_counter = g_battle_gfx_counter;
}
