#include "fft/battle.h"
#include "psx/types.h"

void battle_map_set_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors) {
    battle_map_transition_background_gradient(frame_duration, colors);
}
