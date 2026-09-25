#include "fft/battle.h"
#include "fft/event_require.h"
#include "psx/types.h"

void battle_menu_open_companion_executable_6(void) {
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_DARK_SCREEN);
    battle_menu_request_open_companion_executable(6);
    require_reward_animate_bonus_image_burst();
}
