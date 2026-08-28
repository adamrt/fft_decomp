#include "fft/battle.h"
#include "psx/types.h"

void battle_map_start_ambient_light_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue) {
    battle_map_blend_ambient_light_color(mode, frame_duration, red, green, blue);
}
