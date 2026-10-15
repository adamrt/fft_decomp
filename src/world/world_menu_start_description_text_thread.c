#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Open the pending menu description in a Help dialog at the requested position.
 * Grid-menu ids above 0x1ffff are latched without opening a text thread. */
void world_menu_start_description_text_thread(s32 position) {

    if (g_world_menu_description_text_id > 0x1ffff) {
        g_world_grid_menu_id = g_world_menu_description_text_id;
        return;
    }
    if (g_world_menu_description_text_id > 0) {
        thread_start(THREAD_SCREEN_SLOT_TEXT, text_run_dialog_thread_variant);
        thread_set_parameters(THREAD_SCREEN_SLOT_TEXT, position + (DIALOG_BOX_TYPE_HELP | DIALOG_OPTIONS_NO_TAIL),
            g_world_menu_description_text_id, 0);
        g_menu_input_lock_mode = 1;
    }
}
