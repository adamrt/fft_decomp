#include "fft/battle.h"

void battle_map_freeze(void) {
    battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}
