#include "fft/require.h"
#include "psx/pad.h"
#include "psx/types.h"

void require_input_wait_frames_or_skip(s32 frames) {
    volatile s32 reserved_stack_words[2];
    s32 i = 0;
    u32* input;

    if (frames > 0) {
        do {
            battle_thread_yield();
            input = battle_script_get_controller_input_pointer(0);
            g_require_input_controller = input;
            if ((*input & PSX_PAD_CIRCLE) == 0) {
                i++;
                if (i < frames) {
                    continue;
                }
            }
            break;
        } while (1);
    }
}
