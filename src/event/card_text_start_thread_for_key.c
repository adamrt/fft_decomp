#include "fft/battle_text.h"
#include "fft/card.h"
#include "fft/thread.h"
#include "psx/types.h"

s32 card_text_start_thread_for_key(s32 thread_id, s32 parameter_0, s32 key, s32 parameter_2, s32 thread_field_5c) {
    s32 status = battle_thread_is_running(thread_id);

    if (status == 0) {
        if (g_card_thread_active_key == key) {
            g_card_thread_active_key = 0;
            return 0;
        }
    }
    if (g_card_thread_active_key == 0) {
        battle_thread_start(thread_id, battle_text_character_handling_thread);
        battle_thread_set_parameters(thread_id, parameter_0, key, parameter_2);
        g_card_thread_active_key = key;
        g_battle_threads[thread_id].task_words[3] = thread_field_5c;
        return 1;
    }
    return status;
}
