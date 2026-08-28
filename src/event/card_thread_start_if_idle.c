#include "fft/battle_text.h"
#include "psx/types.h"

void card_thread_start_if_idle(s32 thread_id, s32 parameter_0, s32 parameter_1, s32 parameter_2) {
    if (!battle_thread_is_running(thread_id)) {
        battle_thread_start(thread_id, battle_text_character_handling_thread);
        battle_thread_set_parameters(thread_id, parameter_0, parameter_1, parameter_2);
    }
}
