#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Item menu "equipped by" view: browse the formation units that have the
 * highlighted item equipped.
 *
 * The first call rebuilds the formation list filtered by that item and turns
 * on unit browsing and the banner. Cross or Circle (once the comparison
 * banner thread is idle) restores the full list and the selected unit, stops
 * the three panel threads and returns MENU_ITEM_LIST_STATE_BROWSE; until then
 * the result is MENU_ITEM_LIST_STATE_EQUIPPED_BY.
 */
s32 world_item_show_units_equipping_selected(void) {
    if (g_world_item_equipping_units_initialized == 0) {
        world_formation_save_records_to_party();
        g_world_formation_unit_count = world_formation_build_record_list(
            g_item_list_entries[g_world_menu_cursor_position], g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
        g_world_menu_description_text_id = MENU_GRID_HELP_UNIT_BROWSE;
        g_world_formation_unit_browse_enabled = 1;
        g_world_formation_unit_banner_enabled = 1;
        world_menu_toggle_preview_stats_window(0);
        g_world_item_saved_selected_unit_index = g_world_formation_selected_unit_index;
        g_world_formation_selected_unit_index = 0;
        world_formation_stage_selected_unit();
        g_world_item_equipping_units_initialized = 1;
        menu_set_vsync_mode_and_event_speed(0);
        g_world_formation_unit_cycle_mode = 2;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0) {
        if ((g_menu_input_newly_pressed & PSX_PAD_CROSS) || (g_menu_input_newly_pressed & PSX_PAD_CIRCLE)) {
            g_world_formation_unit_browse_enabled = 0;
            g_world_formation_unit_banner_enabled = 0;
            g_world_formation_unit_count
                = world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
            g_world_formation_selected_unit_index = g_world_item_saved_selected_unit_index;
            world_formation_stage_selected_unit();
            menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
            menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
            menu_request_thread_stop(THREAD_SCREEN_SLOT_UNIT_STATUS);
            g_world_item_equipping_units_initialized = 0;
            g_world_formation_unit_cycle_mode = 0;
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
            return MENU_ITEM_LIST_STATE_BROWSE;
        }
    }
    return MENU_ITEM_LIST_STATE_EQUIPPED_BY;
}
