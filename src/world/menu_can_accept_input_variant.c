#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Returns 1 when the current menu thread may act on input.
 *
 * A help box (box type 0x30) in the text or message slot blocks input and
 * arms a five-frame block. Otherwise input is allowed while the previous
 * thread slot is idle, no stop is requested (parameter 3), no text task or
 * overlay is active and the block has run out.
 */
s32 menu_can_accept_input_variant(void) {
    if ((thread_is_running_asm_variant(THREAD_SCREEN_SLOT_TEXT)
            && (g_thread_contexts[THREAD_SCREEN_SLOT_TEXT].function_parameter_1 & 0x70) == 0x30)
        || (thread_is_running_asm_variant(THREAD_SCREEN_SLOT_MESSAGE)
            && (g_thread_contexts[THREAD_SCREEN_SLOT_MESSAGE].function_parameter_1 & 0x70) == 0x30)) {
        g_menu_input_block_frames = 5;
        return 0;
    }
    if (thread_is_previous_running_variant() == 0) {
        if (thread_get_current_parameter_3_variant() == 0) {
            if (g_menu_input_lock_mode == 0) {
                if (g_menu_help_open == 0) {
                    if (g_menu_input_block_frames == 0) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}
