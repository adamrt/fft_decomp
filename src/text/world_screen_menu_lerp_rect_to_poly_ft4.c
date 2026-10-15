#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Interpolate the four corners of a textured quad between two RECTs. The menu-screen library's copy of
 * menu_lerp_rect_to_poly_ft4; world_menu_animate_window_quads_to_rects calls this one. */
void world_screen_menu_lerp_rect_to_poly_ft4(POLY_FT4* poly, const RECT* from, const RECT* to, s32 t) {
    setXY4(poly, world_screen_menu_lerp_fixed12(from->x, to->x, t), world_screen_menu_lerp_fixed12(from->y, to->y, t),
        world_screen_menu_lerp_fixed12(from->x + from->w, to->x + to->w, t),
        world_screen_menu_lerp_fixed12(from->y, to->y, t), world_screen_menu_lerp_fixed12(from->x, to->x, t),
        world_screen_menu_lerp_fixed12(from->y + from->h, to->y + to->h, t),
        world_screen_menu_lerp_fixed12(from->x + from->w, to->x + to->w, t),
        world_screen_menu_lerp_fixed12(from->y + from->h, to->y + to->h, t));
}
