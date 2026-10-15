#include "fft/battle_map.h"
#include "psx/types.h"

/* map_blend_background_gradient_color with the color offsets narrowed to
 * s16: the entry the event script and the effect timelines call. */
void map_modify_background_gradient(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue) {
    map_blend_background_gradient_color(mode, frame_duration, red, green, blue);
}
