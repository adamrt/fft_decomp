#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Set a WORLD draw mode from aligned texture-page coordinates in a rect. */
void world_gfx_set_draw_mode_from_rect(void* mode, const u16* rect) {
    SetDrawMode((DR_MODE*)mode, 0, 0, GetTPage(0, 0, (s16)(rect[0] & 0xffc0), (s16)(rect[1] & 0xff00)) & 0xffff,
        &g_world_gfx_texture_window);
}
