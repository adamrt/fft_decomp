#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Sibling of world_card_update_menu_request driven by g_world_formation_open_request_state. */
void world_formation_update_open_request(void) {
    s16 state = g_world_formation_open_request_state;
    s32 i;

    if (state == 0) {
        return;
    }
    if (state == 1) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_formation_open_request_state++;
        return;
    }
    if (state == 2) {
        if (!(g_main_system_flags & 8)) {
            g_world_formation_open_request_state = state + 1;
        }
        return;
    }
    if (state == 3) {
        world_formation_reset_menu_context();
        world_menu_set_brightness(0x80, 0x80, 0x80);
        world_formation_run_screen(0, 0);
        world_formation_reset_menu_context();
        g_world_menu_screen_open_request = 0;
        world_menu_start_thread_group(g_world_menu_thread_group_parameter, g_world_menu_location_id,
            g_world_menu_location_window_x, g_world_menu_location_window_y);
        /* The target also passes a1/a2 window offsets to this one-argument callee. */
        ((void (*)(s16, s32, s32))world_menu_open_entry_window)(g_world_menu_open_entry_index, -0x30, -0x38);
        g_world_menu_thread_menu_data = g_world_menu_default_entries;
        world_formation_save_and_set_system_flags();
        wldcore_clear_screen_and_set_map_clip_rect(0);
        for (i = 0x77FF; i >= 0; i--) {
            g_wldcore_scratch_buffer[i] = 0;
        }
        g_world_formation_open_request_state++;
        world_menu_set_brightness(0, 0, 0);
        return;
    }
    if (state < 8) {
        g_world_formation_open_request_state = state + 1;
        return;
    }
    if (state == 8) {
        world_formation_restore_saved_system_flags();
        wldcore_fade_start_screen(0x24, 0x10);
        g_world_formation_open_request_state++;
        return;
    }
    if (state == 9) {
        if (!(g_main_system_flags & 8)) {
            g_world_formation_open_request_state = 0;
        }
    }
}
