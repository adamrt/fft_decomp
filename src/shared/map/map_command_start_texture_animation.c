#include "fft/battle_map.h"
#include "psx/types.h"

/* Start map texture animation value_a from its first frame (map data command
 * 0x83). value_b goes in the object state argument, which that command does not
 * read. Always returns 1. */
s32 map_command_start_texture_animation(s32 value_a, s32 value_b) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_START_TEXTURE_ANIMATION, value_a, value_b, 1);
    return 1;
}
