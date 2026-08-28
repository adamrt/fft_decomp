#include "fft/world.h"

void world_text_run_overlay_thread(void) {
    if (world_thread_find_running_by_task(NATIVE_THREAD_TASK_UNIT_EDITOR_PANEL) != 0
        && world_thread_find_running_by_task(NATIVE_THREAD_TASK_MENU_WINDOW_BUILD) == 0) {
        world_sound_set_effect_to_confirm();
    }
    world_menu_retry_alloc_with_message(0x20000);
    world_bin_load_file(3);
    world_run_battle_help_menu();
}
