#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Tutorial command: advance on any button once the tutorial text thread has
 * ended or has stopped printing. */
void world_script_handle_tutorial_command_wait_for_button(void) {
    s32 running = thread_is_running(WORLD_THREAD_SLOT_TUTORIAL_TEXT);

    if (running == 0) {
        if (g_world_script_tutorial_button_input != 0) {
            g_world_script_tutorial_command_ptr++;
        }
    } else if (world_text_is_printing(WORLD_THREAD_SLOT_TUTORIAL_TEXT) != 1) {
        if (g_world_script_tutorial_button_input != 0) {
            g_world_script_tutorial_command_ptr++;
        }
    }
}
