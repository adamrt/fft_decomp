#include "fft/effect.h"

/* Effect phase handler that switches the map into its specialized destroyed state. */
s32 effect_e456_map_update_specialized_destruction_state(s16 record_index, s32 byte_offset) {
    effect_record_t* record = &g_effect_state_records[record_index];

    switch (record->phase[byte_offset]) {
    case EFFECT_PHASE_IDLE:
        break;
    case EFFECT_PHASE_INIT:
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_RELEASE_GNS_HOLD, 1, 1, 1);
        record->phase[byte_offset] = EFFECT_PHASE_UPDATE;
        break;
    case EFFECT_PHASE_UPDATE:
        battle_script_set_specialized_map_destroyed();
        break;
    case EFFECT_PHASE_DESTROY:
        record->phase[byte_offset] = EFFECT_PHASE_IDLE;
        break;
    }
}
