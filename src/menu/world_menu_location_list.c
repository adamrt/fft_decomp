/* Visible location list and destination confirm. */

#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Build the list of visible world locations (script variables 0x200..0x22a,
 * rows of location-name text 0x9000 + location) and position the menu around
 * the current one. `selected` stays unset when the current location is not
 * listed. */
void world_menu_build_location_list(void) {
    s32 i;
    s32 count;
    s32 selected;

    for (i = 0, count = 0; i < WORLD_LOCATION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) != 0) {
            g_world_menu_location_list_text_ids[count] = i + (s16)TEXT_ID_LOCATION_NAME_BASE;
            if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_COLOR_FIRST) == 0) {
                g_world_menu_location_list_row_colors[count] = 8;
            } else {
                g_world_menu_location_list_row_colors[count] = 0;
            }
            if (i == script_get_variable(EVENT_SCRIPT_VAR_LOCATION)) {
                g_world_menu_location_list_row_colors[count] = 4;
                selected = count;
            }
            count++;
        }
    }
    if (count >= 8) {
        g_world_menu_location_list_layout.columns.row_count = 7;
        g_world_menu_location_list_layout.columns.hidden_rows = count - 7;
    } else {
        g_world_menu_location_list_layout.columns.row_count = count;
        g_world_menu_location_list_layout.columns.hidden_rows = 0;
    }
    if (g_world_menu_location_list_layout.columns.hidden_rows < selected) {
        g_world_menu_location_list_layout.row_offset = g_world_menu_location_list_layout.columns.hidden_rows;
    } else {
        g_world_menu_location_list_layout.row_offset = selected;
    }
    g_menu_thread_menu_data[6].selected_index = selected;
    g_menu_thread_menu_data[6].window.y = (7 - g_world_menu_location_list_layout.columns.row_count) * 8 - 0x48;
    menu_run_scrolling_ability_list_thread_variant();
}

/* Slot 0 is stored through a plain pointer on purpose: `pending[0]` is a
 * scalar reference there, so the scheduler may sink it below the text-id
 * load as in the target. Writing g_menu_pending_selection[0] marks the
 * store as an array element and keeps it above that load. */
void world_map_confirm_destination(void) {
    s32 selection;
    s16 value;

    selection = g_menu_thread_menu_data[6].selected_index;
    if (g_world_menu_location_list_row_colors[selection] != 4) {
        s16* pending = g_menu_pending_selection;

        g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
        pending[0] = 0;
        value = g_world_menu_location_list_text_ids[selection] - (s16)TEXT_ID_LOCATION_NAME_BASE;
        pending[1] = value;
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, value);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_DESTINATION;
        thread_set_parameters(g_thread_current_id + 1, 0, 0, 1);
        thread_set_parameters(g_thread_current_id + 2, 0, 0, 1);
    } else {
        g_sound_pending_sfx_id = MAIN_SFX_INVALID;
    }
    g_menu_text_palette_offset = 0;
    thread_exit_current();
}
