#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* The thread EVENT_OPCODE_DARK_SCREEN starts: loads REQUIRE.OUT and runs its bonus image burst. */
void script_dark_screen_thread_variant(void) {
    thread_set_current_task_id(THREAD_TASK_DARK_SCREEN);
    menu_request_open_companion_executable(COMPANION_OVERLAY_REQUIRE);
    require_reward_animate_bonus_image_burst();
}
