#include "fft/wldcore.h"
#include "psx/types.h"

/* Sort the two screen-fade boxes into the active ordering table at their
 * priorities. A box is drawn while its fade runs or it is flagged visible,
 * and only when its color is not black. */
void wldcore_fade_draw_screen_overlays(void) {
    u32* flags;
    GsBOXF* boxes;
    s32* priorities;
    s32 i;

    flags = g_wldcore_screen_fade_state.flags;
    boxes = g_wldcore_screen_fade_state.boxes;
    for (i = 0; i < 2; i++) {
        /* Reloaded inside the loop so that the priority walk, not its invariant
         * copy, holds the register the exit test compares. */
        priorities = g_wldcore_screen_fade_state.priorities;
        if (flags[i] & (WLDCORE_SCREEN_FADE_RUNNING | WLDCORE_SCREEN_FADE_BOX_VISIBLE)) {
            if (g_wldcore_screen_fade_state.boxes[i].r != 0 || g_wldcore_screen_fade_state.boxes[i].g != 0
                || g_wldcore_screen_fade_state.boxes[i].b != 0) {
                GsSortBoxFill(&boxes[i], &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index], priorities[i]);
            }
        }
    }
}
