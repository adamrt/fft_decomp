#include "fft/battle_map.h"
#include "psx/types.h"

/* Fade the map darkness to `color` without changing the base color, unlike
 * map_transition_and_store_darkness_color. */
void map_set_darkness_color(s32 frame_duration, const CVECTOR* color) {
    map_transition_darkness_color(frame_duration, color);
}
