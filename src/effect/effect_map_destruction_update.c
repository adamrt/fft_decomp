#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/effect.h"
#include "fft/script.h"
#include "psx/types.h"

/* Release GNS dispatch hold and advance the map-destruction handoff. */
s32 effect_map_destruction_update(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame) {
    s16 state_index = record_index;
    effect_record_t* record = &g_effect_state_records[state_index];

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;
    case EFFECT_CALLBACK_PHASE_INIT:
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_RELEASE_GNS_HOLD, 1, 1, 1);
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;
    case EFFECT_CALLBACK_PHASE_UPDATE:
        script_set_specialized_map_destroyed();
        break;
    case EFFECT_CALLBACK_PHASE_DESTROY:
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
