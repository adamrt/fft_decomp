#include "fft/effect.h"
#include "fft/map.h"
#include "psx/types.h"

void battle_map_command_get_texture_animation_active(s32 value) {
    battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE, value, 1, 1);
}
