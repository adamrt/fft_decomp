#include "fft/battle_map.h"
#include "psx/types.h"

/* map_blend_darkness_color with the color offsets narrowed to s16. The
 * arguments are the mode, the frame duration and the red, green and blue offsets. */
void map_start_darkness_blend(s32 first, s32 second, s16 third, s16 fourth, s16 fifth) {
    map_blend_darkness_color(first, second, third, fourth, fifth);
}
