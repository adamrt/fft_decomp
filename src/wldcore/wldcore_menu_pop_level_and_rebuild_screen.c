#include "fft/wldcore.h"
#include "psx/types.h"

/* Tears down the top window record: releases it, clears the two window system
 * flags, hands the current location id to wldcore_map_build_location_menu_entries, and wipes the
 * scratch buffer.
 *
 * The local pointer to g_wldcore_map_projection_state.marker.kind reproduces the target's
 * saved-register base: read directly, the global is re-materialised at each of
 * the two uses. */
void wldcore_menu_pop_level_and_rebuild_screen(void) {
    s32* selected = &g_wldcore_map_projection_state.marker.kind;
    s32 i;

    wldcore_window_set_render_objects_visible(1);
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth - 1;
    g_main_system_flags &= ~0x3000;
    wldcore_menu_dispatch_publish_handler();
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count
        = wldcore_map_build_location_menu_entries(
            *selected, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entries);
    wldcore_menu_push_location_menu_level(*selected);
    wldcore_fade_start_screen(4, 0x10);
    wldcore_clear_screen_and_set_map_clip_rect(0);
    for (i = 0x77ff; i >= 0; i--) {
        g_wldcore_scratch_buffer[i] = 0;
    }
}
