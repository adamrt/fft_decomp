#include "fft/effect.h"
#include "fft/map.h"

s32 effect_e454_map_set_3d_objects_to_state_2_state(s16 record_index, s32 byte_offset) {
    effect_record_t* record = &g_effect_state_records[record_index];

    switch (record->phase[byte_offset]) {
    case EFFECT_PHASE_IDLE:
        break;
    case EFFECT_PHASE_INIT:
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 1, 2, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 2, 2, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 3, 2, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 4, 2, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 5, 2, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 6, 2, 1);
        record->phase[byte_offset] = EFFECT_PHASE_UPDATE;
        break;
    case EFFECT_PHASE_UPDATE:
        break;
    case EFFECT_PHASE_DESTROY:
        record->phase[byte_offset] = EFFECT_PHASE_IDLE;
        break;
    }
}
