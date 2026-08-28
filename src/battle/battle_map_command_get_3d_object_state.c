#include "fft/effect.h"
#include "fft/map.h"
#include "psx/types.h"

void battle_map_command_get_3d_object_state(s32 value) {
    battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_GET_3D_OBJECT_STATE, value, 1, 1);
}
