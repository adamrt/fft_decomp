#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Read the menu input (tutorial or pad) and filter it: nothing while the
 * item category change, the status banner or the lockout timer is busy, and
 * no Select or Start (mask 0xF0FF) while a help text is requested or
 * showing, or while the dismiss menu's message runs. */
s32 world_input_filter_menu(void) {
    s32 input = menu_input_read_tutorial_or_controller(0);

    if (g_world_item_category_input_lock != 0 || g_world_unit_status_banner_active != 0
        || world_input_get_lockout_timer() != 0) {
        input = 0;
    } else if (thread_is_running(THREAD_SCREEN_SLOT_TEXT) != 0) {
        if (g_world_formation_current_menu == WORLD_FORMATION_MENU_DISMISS_UNIT) {
            input &= 0xF0FF;
        }
    } else if (g_menu_help_message_text_id != 0 || g_world_grid_menu_id != MENU_GRID_HELP_NONE) {
        input &= 0xF0FF;
    }
    return input;
}
