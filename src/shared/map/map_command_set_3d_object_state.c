#include "fft/battle_map.h"
#include "psx/types.h"

/* Event Use3DObject: start state row value_b of map mesh part value_a (map data
 * command 0x80, with flag 1). Always returns 1. */
s32 map_command_set_3d_object_state(s32 value_a, s32 value_b) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, value_a, value_b, 1);
    return 1;
}
