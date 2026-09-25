#include "fft/world.h"
#include "psx/types.h"

void world_menu_input_loop_thread(void) {
    world_menu_entry_t* parameter = world_thread_get_current_parameter_1();

    g_world_menu_sound_muted = 1;
    for (;;) {
        world_thread_yield();
        if (world_menu_check_thread_completion(&g_world_menu_new_button_input) != 0) {
            break;
        }
        world_menu_handle_entry_confirm(parameter, 0);
        world_menu_cancel_thread_group((world_menu_cancel_context_t*)parameter);
    }
    world_thread_yield();
    g_world_menu_sound_muted = 0;
    if (world_thread_get_current_parameter_2() == 0) {
        world_thread_exit_current();
    }
}
