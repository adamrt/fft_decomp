#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

void map_freeze(void) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}
