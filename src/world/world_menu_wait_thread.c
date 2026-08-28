#include "fft/world.h"
#include "psx/types.h"

void world_menu_wait_thread(void) {
    s32 entry = (s32)world_thread_get_current_parameter_1();

    g_world_menu_wait_row_actions[0] = -2;
    g_world_menu_wait_row_actions[1] = -2;
    world_menu_confirm_action_silently(entry);
    world_thread_exit_current();
}
