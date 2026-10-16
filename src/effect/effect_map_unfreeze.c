/* An effect callback that unfreezes the map when its keyframe starts. */

#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/effect.h"
#include "psx/types.h"

/* Callback handler: unfreezes the map when its keyframe starts. */
s32 effect_map_unfreeze(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame) {
    s16 state_index = record_index;
    effect_record_t* record;

    record = &g_battle_effect_state_records[state_index];

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;
    case EFFECT_CALLBACK_PHASE_INIT:
        battle_map_unfreeze();
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;
    case EFFECT_CALLBACK_PHASE_UPDATE:
        break;
    case EFFECT_CALLBACK_PHASE_DESTROY:
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
