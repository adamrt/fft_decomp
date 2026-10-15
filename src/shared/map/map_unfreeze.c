#include "fft/battle_map.h"

/* Resume the map texture animations paused by map_freeze. */
void map_unfreeze(void) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}
