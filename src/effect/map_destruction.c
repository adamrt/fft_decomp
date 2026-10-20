/* An effect callback that releases the GNS dispatch hold and advances the map destruction. */

#include "fft/battle_map.h"
#include "fft/effect.h"
#include "fft/effect_script.h"
#include "fft/script.h"
#include "psx/types.h"

/* Callback handler for the map-destruction effect. Its first call releases the
 * GNS data a held map append loaded (BATTLE_MAP_LOAD_COMMAND_START_HELD); every later
 * call reports the map destroyed, moving the map-destruction wait state from 1
 * (append finished) to 2, after which the event request loop releases the hold
 * again and polls the load to its end. */
s32 effect_map_destruction_update(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame) {
    s16 state_index = record_index;
    effect_record_t* record = &g_effect_state_records[state_index];

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;
    case EFFECT_CALLBACK_PHASE_INIT:
        battle_map_data_command_dispatch(BATTLE_MAP_DATA_COMMAND_RELEASE_GNS_HOLD, 1, 1, 1);
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;
    case EFFECT_CALLBACK_PHASE_UPDATE:
        script_specialized_map_destroyed_set();
        break;
    case EFFECT_CALLBACK_PHASE_DESTROY:
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
