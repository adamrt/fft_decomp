#include "fft/battle.h"
#include "fft/thread.h"
#include "psx/types.h"

void battle_menu_mini_menu_cancel_delay_thread(void) {
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_MENU_SOUND_DELAY);
    battle_thread_wait_frames(2);
    g_battle_menu_pending_selection[0] = 7;
    battle_thread_exit_current();
}
