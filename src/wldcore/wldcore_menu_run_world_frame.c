#include "fft/gfx.h"
#include "fft/sound.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/*
 * Runs the WORLD menu code for one world-map frame, drawing into `context`.
 *
 * Skipped while MAIN_SYSTEM_FLAG_SKIP_MENU_FRAME is set. While the unit panel
 * threads are in use it draws the window frames, runs the event input update
 * and plays the frame's sound effect itself; otherwise it runs the formation
 * menu frame. Buttons in g_wldcore_menu_blocked_buttons are withheld from
 * the menus.
 */
void wldcore_menu_run_world_frame(GsOT* context) {
    u32* otag_entries;
    u32* sprite_entries;
    u32* menu_entries;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_SKIP_MENU_FRAME) {
        return;
    }
    world_formation_set_ordering_table_pointer((u32*)(context->org + 15));
    world_formation_set_menu_allowed_buttons(~g_wldcore_menu_blocked_buttons);
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS) {
        otag_entries = (u32*)context->org;
        g_gfx_current_otag_entry = otag_entries + g_wldcore_menu_ordering_table_offset;
        menu_draw_active_window_frames();
        sprite_entries = (u32*)context->org;
        world_input_update_event_state(sprite_entries + g_wldcore_menu_ordering_table_offset + 2,
            g_wldcore_input_current_buttons, g_active_graphics_buffer_index * 240);
        if (g_sound_pending_sfx_id != -1) {
            wldcore_sound_play_effect(g_sound_pending_sfx_id);
        }
    } else {
        menu_entries = (u32*)context->org;
        world_formation_run_menu_frame(
            menu_entries + g_wldcore_menu_ordering_table_offset, g_wldcore_input_current_buttons);
    }
}
