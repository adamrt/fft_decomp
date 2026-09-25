#include "fft/world.h"
#include "psx/types.h"

/* Slot 0 is stored through a plain pointer on purpose: `pending[0]` is a
 * scalar reference there, so the scheduler may sink it below the text-id
 * load as in the target. Writing g_world_menu_pending_selection[0] marks the
 * store as an array element and keeps it above that load. */
void world_map_confirm_destination(void) {
    s32 selection;
    s16 value;

    selection = g_world_menu_thread_menu_data[6].selected_index;
    if (g_world_menu_location_list_row_colors[selection] != 4) {
        s16* pending = g_world_menu_pending_selection;

        g_world_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
        pending[0] = 0;
        value = g_world_menu_location_list_text_ids[selection] + 0x7000;
        pending[1] = value;
        world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, value);
        g_main_system_flags |= 0x4000;
        world_thread_set_parameters(g_world_thread_current_id + 1, 0, 0, 1);
        world_thread_set_parameters(g_world_thread_current_id + 2, 0, 0, 1);
    } else {
        g_world_sound_effect_id_to_play = MAIN_SFX_INVALID;
    }
    g_world_menu_text_color = 0;
    world_thread_exit_current();
}
