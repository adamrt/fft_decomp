#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_handle_tutorial_command_wait_for_button(void) {
    s32 running = world_thread_is_running(0x10);

    if (running == 0) {
        if (g_world_script_tutorial_button_input != 0) {
            g_world_script_tutorial_command_ptr++;
        }
    } else if (world_text_is_printing(0x10) != 1) {
        if (g_world_script_tutorial_button_input != 0) {
            g_world_script_tutorial_command_ptr++;
        }
    }
}
