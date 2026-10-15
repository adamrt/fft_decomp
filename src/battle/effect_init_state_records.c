#include "fft/battle.h"
#include "fft/battle_effect.h"

void effect_init_state_records(void) {
    s32 i;

    g_effect_state_records[0].next_index = 0;
    g_effect_available_record_head = 1;
    for (i = 1; i < EFFECT_STATE_RECORD_CAPACITY; i++) {
        g_effect_state_records[i].record_index = i;
        g_effect_state_records[i].next_index = i + 1;
        g_effect_state_records[i].last_native_callback = 0;
    }
    g_effect_state_records[i].next_index = 0;
    g_effect_state_records[i].record_index = i;
    g_effect_allocated_record_head = 0;
    g_effect_current_record_index = 0;
    g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_IDLE;
}
