#include "fft/battle_map.h"

/* Fade the background gradient to `colors` and make them the base pair that the
 * MAP_COLOR_MODE_BASE modes and RESTORE start from. */
void map_transition_and_store_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors) {
    map_transition_background_gradient(frame_duration, colors);
    g_map_background_gradient_transition.colors[0] = colors->colors[MAP_BACKGROUND_GRADIENT_TOP];
    g_map_background_gradient_transition.colors[1] = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
}
