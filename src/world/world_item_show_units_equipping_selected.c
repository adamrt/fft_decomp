#include "fft/data.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

s32 world_item_show_units_equipping_selected(void) {
    if (g_world_item_equipping_units_initialized == 0) {
        world_formation_save_records_to_party_data();
        g_world_formation_unit_count = world_formation_build_record_list(
            g_world_menu_entry_ids[g_world_menu_cursor_position], g_world_formation_unit_pointers, 0);
        g_world_menu_description_text_id = 0x20002;
        g_world_formation_unit_browse_enabled = 1;
        g_world_formation_unit_banner_enabled = 1;
        world_menu_toggle_preview_stats_window(0);
        g_world_item_saved_selected_unit_index = g_world_formation_selected_unit_index;
        g_world_formation_selected_unit_index = 0;
        world_formation_stage_selected_unit();
        g_world_item_equipping_units_initialized = 1;
        world_script_set_vsync_mode_and_event_speed(0);
        g_world_formation_unit_cycle_mode = 2;
    }
    if (world_thread_is_running(7) == 0) {
        if ((g_world_input_newly_pressed & PSX_PAD_CROSS) || (g_world_input_newly_pressed & PSX_PAD_CIRCLE)) {
            g_world_formation_unit_browse_enabled = 0;
            g_world_formation_unit_banner_enabled = 0;
            g_world_formation_unit_count = world_formation_build_record_list(0, g_world_formation_unit_pointers, 0);
            g_world_formation_selected_unit_index = g_world_item_saved_selected_unit_index;
            world_formation_stage_selected_unit();
            world_menu_stop_unit_status_banner_thread(9);
            world_menu_stop_unit_status_banner_thread(0xC);
            world_menu_stop_unit_status_banner_thread(8);
            g_world_item_equipping_units_initialized = 0;
            g_world_formation_unit_cycle_mode = 0;
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
            return 0;
        }
    }
    return 6;
}
