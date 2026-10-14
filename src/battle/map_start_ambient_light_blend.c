#include "fft/battle_map.h"
#include "psx/types.h"

/* map_blend_ambient_light_color with the color offsets narrowed to s16:
 * the entry event MapDarkness calls. */
void map_start_ambient_light_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue) {
    map_blend_ambient_light_color(mode, frame_duration, red, green, blue);
}
