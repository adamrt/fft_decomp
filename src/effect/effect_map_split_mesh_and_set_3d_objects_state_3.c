/* An effect callback that splits the map's mesh parts and sets 3D objects 1-8 to state 3. */

#include "fft/battle_map.h"
#include "fft/effect.h"
#include "fft/effect_script.h"
#include "psx/types.h"

/* Callback handler: when its keyframe starts, splits the map's mesh parts and
 * sets 3D objects 1-8 to state 3. */
s32 effect_map_split_mesh_and_set_3d_objects_state_3(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame) {
    s16 state_index = record_index;
    effect_record_t* record = &g_effect_state_records[state_index];

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;
    case EFFECT_CALLBACK_PHASE_INIT:
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SPLIT_MESH_PARTS, 1, 1, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 1, 3, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 2, 3, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 3, 3, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 4, 3, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 5, 3, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 6, 3, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 7, 3, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 8, 3, 1);
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;
    case EFFECT_CALLBACK_PHASE_UPDATE:
        break;
    case EFFECT_CALLBACK_PHASE_DESTROY:
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
