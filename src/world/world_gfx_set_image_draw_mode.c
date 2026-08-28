#include "fft/world.h"
#include "psx/gpu.h"

/* Select the WORLD image texture page and initialize its draw mode. */
void world_gfx_set_image_draw_mode(DR_MODE* mode, s32 image_type) {
    if (image_type == 0) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 960, 256), &g_world_gfx_texture_window);
    } else if (image_type == 1) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 448, 0), &g_world_gfx_texture_window);
    } else if (image_type == 2) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 384, 0), &g_world_gfx_texture_window);
    } else if (image_type == 3) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 832, 256), &g_world_gfx_texture_window);
    } else {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 896, 288), &g_world_gfx_texture_window);
    }
}
