#include "fft/world.h"
#include "psx/types.h"

s32 world_menu_is_input_allowed(void) {
    if ((world_thread_is_running_80100164(1) && (g_world_thread_contexts[1].function_parameter_1 & 0x70) == 0x30)
        || (world_thread_is_running_80100164(2) && (g_world_thread_contexts[2].function_parameter_1 & 0x70) == 0x30)) {
        g_world_menu_input_block_frames = 5;
        return 0;
    }
    if (world_thread_is_previous_running() == 0) {
        if (world_thread_get_current_parameter_3() == 0) {
            if (g_world_thread_task_active == 0) {
                if (g_world_text_overlay_active == 0) {
                    if (g_world_menu_input_block_frames == 0) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}
