#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

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
