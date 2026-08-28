#include "fft/effect.h"

s32 effect_e450_map_step_unfreeze_state(s16 record_index, s32 byte_offset) {
    effect_record_t* record;
    effect_state_t* state;

    record = &g_effect_state_records[record_index];
    state = (effect_state_t*)((u8*)record + byte_offset);

    switch (state->phase) {
    case EFFECT_PHASE_IDLE:
        break;
    case EFFECT_PHASE_INIT:
        battle_map_unfreeze();
        state->phase = EFFECT_PHASE_UPDATE;
        break;
    case EFFECT_PHASE_UPDATE:
        break;
    case EFFECT_PHASE_DESTROY:
        state->phase = EFFECT_PHASE_IDLE;
        break;
    }
}
