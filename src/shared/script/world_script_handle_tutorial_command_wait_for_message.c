#include "fft/thread.h"
#include "fft/world.h"

/* Tutorial command: let the tutorial text continue and move on once its
 * thread has ended. */
void world_script_handle_tutorial_command_wait_for_message(void) {
    world_text_resume_printing(WORLD_THREAD_SLOT_TUTORIAL_TEXT);
    if (thread_is_running(WORLD_THREAD_SLOT_TUTORIAL_TEXT) == 0) {
        g_world_script_tutorial_command_ptr++;
    }
}
