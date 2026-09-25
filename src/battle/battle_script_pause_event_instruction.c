#include "fft/battle.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_script_pause_event_instruction(void) {
    do {
        battle_thread_yield();
        if (battle_thread_get_current_task_id() == NATIVE_THREAD_TASK_STOP_REQUEST) {
            return;
        }
    } while ((g_battle_script_event_input & (PSX_PAD_SELECT | PSX_PAD_CROSS | PSX_PAD_CIRCLE)) == 0);
}
