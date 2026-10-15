#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Event ColorField: tint or fade all 256 colors of the map CLUT row with
 * map_modify_palette. `first` is its color mode (map_color_mode_e) and
 * `second` its amount: 0 applies the colors at once, another value fades to
 * them. */
void map_color_field(s32 first, s32 second, s16 red, s16 green, s16 blue) {
    map_modify_palette(first, second, MAP_CLUT_ROW_MAP, 0, 1, red, green, blue);
}
