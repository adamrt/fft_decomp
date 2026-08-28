#include "fft/effect.h"
#include "fft/map.h"
#include "psx/types.h"

s32 battle_map_command_set_3d_object_state(s32 value_a, s32 value_b) {
    battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, value_a, value_b, 1);
    return 1;
}
