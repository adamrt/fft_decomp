#include "fft/battle_map.h"
#include "psx/types.h"

/* Fade the ambient light to `color` without changing the base color, unlike
 * map_transition_and_store_ambient_light_color. */
void map_set_ambient_light_color(s32 frame_duration, const CVECTOR* color) {
    map_transition_ambient_light_color(frame_duration, color);
}
