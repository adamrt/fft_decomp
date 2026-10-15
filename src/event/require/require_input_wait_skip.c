#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Yield until Circle is newly pressed, at least one frame. */
void require_input_wait_skip(void) {
    u32* input;

    do {
        thread_yield();
        input = script_get_controller_input_pointer(0);
        g_menu_input_controller = input;
    } while ((*input & PSX_PAD_CIRCLE) == 0);
}
