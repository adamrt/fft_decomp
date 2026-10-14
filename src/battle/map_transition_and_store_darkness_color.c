#include "fft/battle_map.h"
#include "psx/types.h"

/* Fade the map darkness to `color` and make it the base color that the
 * MAP_COLOR_MODE_BASE modes and RESTORE start from. */
void map_transition_and_store_darkness_color(s32 frame_duration, const CVECTOR* color) {
    map_transition_darkness_color(frame_duration, color);
    g_map_darkness_color = *color;
}
