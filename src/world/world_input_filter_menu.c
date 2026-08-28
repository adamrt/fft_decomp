#include "fft/world.h"
#include "psx/types.h"

s32 world_input_filter_menu(void) {
    /* The target passes a0 = 0 to this argument-less reader. */
    s32 input = ((s32 (*)(s32))world_input_read_tutorial_or_controller)(0);

    if (g_world_item_category_input_lock != 0 || g_world_unit_status_banner_active != 0
        || world_input_get_lockout_timer() != 0) {
        input = 0;
    } else if (world_thread_is_running(1) != 0) {
        if (g_world_formation_current_menu == 4) {
            input &= 0xF0FF;
        }
    } else if (g_world_menu_description_text_id != 0 || g_world_grid_menu_id != 0) {
        input &= 0xF0FF;
    }
    return input;
}
