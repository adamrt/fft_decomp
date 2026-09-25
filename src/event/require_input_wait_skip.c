#include "fft/event_require.h"
#include "psx/pad.h"
#include "psx/types.h"

void require_input_wait_skip(void) {
    u32* input;

    do {
        battle_thread_yield();
        input = battle_script_get_controller_input_pointer(0);
        g_require_input_controller = input;
    } while ((*input & PSX_PAD_CIRCLE) == 0);
}
