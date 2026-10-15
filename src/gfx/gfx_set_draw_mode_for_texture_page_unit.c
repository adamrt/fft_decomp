#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "psx/types.h"

void gfx_set_draw_mode_for_texture_page(DR_MODE* mode, s32 page) {
    if (page == MENU_TEXTURE_PAGE_FRAME) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x3c0, 0x100), &g_gfx_texture_window);
    } else if (page == MENU_TEXTURE_PAGE_ALLOCATED_IMAGES) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x1c0, 0), &g_gfx_texture_window);
    } else if (page == MENU_TEXTURE_PAGE_TEXT_IMAGES) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x180, 0), &g_gfx_texture_window);
    } else if (page == MENU_TEXTURE_PAGE_UNIT_SPRITES) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x340, 0x100), &g_gfx_texture_window);
    } else {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x380, 0x120), &g_gfx_texture_window);
    }
}

/* Set a draw mode from aligned texture-page coordinates from the menu text image origin. */
void gfx_set_draw_mode_from_origin(DR_MODE* mode, const point16_t* origin) {
    SetDrawMode(mode, 0, 0, GetTPage(0, 0, origin->x & ~0x3f, origin->y & ~0xff), &g_gfx_texture_window);
}

/* Returns 0 in BATTLE and WORLD; in BATTLE it sits between
 * gfx_set_draw_mode_from_origin and script_noop_10. MAIN calls BATTLE's copy
 * through main_overlay_call_battle_return_zero while script variable 0xff is
 * set. Nothing else references it; the name is a neutral placeholder until a
 * role turns up. */
s32 script_return_zero_1(void) {
    return 0;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_return_zero_1
 * and script_frame_after_requests_stub. Nothing on the disc calls it or points
 * at it; the name is a neutral placeholder until a reference turns up. */
void script_noop_10(void) {
}

/* Empty. The event frame runner calls it each frame after the pending requests. */
void script_frame_after_requests_stub(void) {
}
