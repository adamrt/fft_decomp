#include "fft/battle.h"
#include "psx/types.h"

void battle_map_set_darkness_color(s32 frame_duration, const map_color_t* color) {
    battle_map_transition_darkness_color(frame_duration, color);
}
