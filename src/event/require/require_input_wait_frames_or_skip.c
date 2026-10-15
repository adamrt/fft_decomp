#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Wait `frames` frames, ending early when Circle is newly pressed. */
void require_input_wait_frames_or_skip(s32 frames) {
    s32 reserved_stack_words[2];
    s32 i = 0;
    u32* input;

    if (frames > 0) {
        do {
            thread_yield();
            input = script_get_controller_input_pointer(0);
            g_menu_input_controller = input;
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
