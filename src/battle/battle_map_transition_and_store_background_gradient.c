#include "fft/battle.h"

void battle_map_transition_and_store_background_gradient(
    s32 frame_duration, const map_background_gradient_colors_t* colors) {
    battle_map_transition_background_gradient(frame_duration, colors);
    g_map_background_gradient_transition.colors[0] = colors->first;
    g_map_background_gradient_transition.colors[1] = colors->second;
}
