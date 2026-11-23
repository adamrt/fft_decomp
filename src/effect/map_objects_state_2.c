/* An effect callback that sets the map's 3D objects 1-6 to state 2 when its keyframe starts. */

#include "fft/effect/effect.h"
#include "fft/effect/effect_script.h"
#include "fft/map/map.h"
#include "psx/types.h"

/* Callback handler: when its keyframe starts, sets the map's 3D objects 1-6 to
 * state 2. */
s32 effect_set_map_3d_objects_state_2(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame) {
    s16 state_index = record_index;
    effect_record_t* record = &g_effect_state_records[state_index];

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;
    case EFFECT_CALLBACK_PHASE_INIT:
        map_dispatch_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 1, 2, 1);
        map_dispatch_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 2, 2, 1);
        map_dispatch_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 3, 2, 1);
        map_dispatch_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 4, 2, 1);
        map_dispatch_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 5, 2, 1);
        map_dispatch_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 6, 2, 1);
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;
    case EFFECT_CALLBACK_PHASE_UPDATE:
        break;
    case EFFECT_CALLBACK_PHASE_DESTROY:
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
