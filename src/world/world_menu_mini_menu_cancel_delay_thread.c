#include "fft/world.h"
#include "psx/types.h"

void world_menu_mini_menu_cancel_delay_thread(void) {
    world_thread_set_current_task_id(NATIVE_THREAD_TASK_MENU_SOUND_DELAY);
    world_thread_wait_frames(2);
    g_world_menu_pending_selection[0] = 7;
    world_thread_exit_current();
}
