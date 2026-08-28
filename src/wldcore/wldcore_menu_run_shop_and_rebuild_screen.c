#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* At full brightness, run the shop / soldier-office loop (0x80133478),
 * then reload the war-funds script variable, reset the parent level's window,
 * rebuild the panel list for the current location, fade in, clear the scratch
 * buffer and restart the world-map music (0x11b).
 *
 * The local alias for the current location id reproduces the target's
 * saved-register base for its two loads. */
void wldcore_menu_run_shop_and_rebuild_screen(s32 file_slot) {
    s32* selected;
    s32* clear;
    s32 i;

    world_menu_set_brightness(0x80, 0x80, 0x80);
    wldcore_sound_wait_for_queue_drain();
    world_shop_run_screen(file_slot);
    g_wldcore_displayed_numeric_value = world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]);
    selected = &g_wldcore_map_projection_state.marker.kind;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count
        = wldcore_map_build_location_menu_entries(
            *selected, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entries);
    wldcore_menu_push_location_menu_level(*selected);
    wldcore_fade_start_screen(4, 0x10);
    wldcore_clear_screen_and_set_map_clip_rect(0);
    i = 0x77FF;
    clear = &g_wldcore_scratch_buffer[0x77FF];
    for (; i >= 0; i--) {
        *clear = 0;
        clear--;
    }
    wldcore_sound_enqueue_music_start(0x11B);
}
