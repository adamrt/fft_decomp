#include "fft/world.h"
#include "psx/types.h"

/* Thread body: loads companion overlay file 7 and runs it with state 3 until
 * it reports completion. */
void world_menu_run_companion_overlay_7_thread(void) {
    if (world_thread_is_running_80100164(3) != 0) {
        world_thread_exit_current();
    }
    if (world_thread_is_running_80100164(1) != 0) {
        world_thread_exit_current();
    }
    world_menu_set_transition_active_flag();
    world_sound_set_effect_to_confirm();
    world_menu_retry_alloc_with_message(0x20000);
    world_bin_load_file(7);
    world_thread_wait_frames(2);
    g_world_companion_overlay_state = 3;
    do {
        g_world_menu_new_button_input = 0;
        world_thread_yield();
    } while (g_world_companion_overlay_state != 0);
    g_world_menu_new_button_input = 0;
    world_menu_free_high_overlay();
    world_thread_wait_frames(2);
    world_menu_clear_transition_active_flag();
    world_thread_exit_current();
}
