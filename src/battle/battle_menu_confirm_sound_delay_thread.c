#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_confirm_sound_delay_thread(void) {
    g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_MENU_SOUND_DELAY);
    battle_thread_wait_frames(0x10);
    battle_thread_exit_current();
}
