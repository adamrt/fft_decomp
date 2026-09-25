#include "fft/battle.h"
#include "psx/types.h"

void battle_map_set_ambient_light_color(s32 frame_duration, const map_color_t* color) {
    battle_map_transition_ambient_light_color(frame_duration, color);
}
