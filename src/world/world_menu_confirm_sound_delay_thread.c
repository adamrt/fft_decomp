#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_confirm_sound_delay_thread(void) {
    g_world_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
    world_thread_set_current_task_id(NATIVE_THREAD_TASK_MENU_SOUND_DELAY);
    world_thread_wait_frames(0x10);
    world_thread_exit_current();
}
