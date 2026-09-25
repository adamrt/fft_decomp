#include "fft/event_equip.h"
#include "psx/types.h"

/* Start the selected text thread only when its scheduler slot is idle.
 *
 * The status call intentionally reuses the incoming thread ID already in
 * $a0; making the argument explicit changes the target instruction schedule. */
void equip_thread_start_if_idle(s32 thread_id, s32 parameter_0, s32 parameter_1, s32 parameter_2) {
    if (((s32 (*)())battle_thread_is_running)() == 0) {
        battle_thread_start(thread_id, battle_text_character_handling_thread);
        battle_thread_set_parameters(thread_id, parameter_0, parameter_1, parameter_2);
    }
}
