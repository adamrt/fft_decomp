#include "fft/world.h"
#include "psx/types.h"

void world_script_handle_tutorial_command_wait_for_message(void) {
    world_text_resume_printing(0x10);
    if (world_thread_is_running(0x10) == 0) {
        g_world_script_tutorial_command_ptr++;
    }
}
