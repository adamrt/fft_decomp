#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Yield until the scheduler leaves task 3 or the player presses one of the
 * 0x160 buttons (event pause instruction at 0x800e7654-0x800e76a8). */
void world_script_pause_event_instruction(void) {
    do {
        world_thread_yield();
    } while (world_thread_get_current_task_id() != NATIVE_THREAD_TASK_STOP_REQUEST
        && (g_world_menu_new_button_input & (PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT)) == 0);
}
