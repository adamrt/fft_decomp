#include "fft/battle_map.h"
#include "psx/types.h"

/* Fade the background gradient to `colors` without changing the base pair, unlike
 * map_transition_and_store_background_gradient. */
void map_set_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors) {
    map_transition_background_gradient(frame_duration, colors);
}
