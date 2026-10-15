/* S16 entries of the gradient, ambient and darkness blends. */

#include "fft/battle_map.h"
#include "psx/types.h"

/* map_blend_background_gradient_color with the color offsets narrowed to
 * s16: the entry the event script and the effect timelines call. */
void map_modify_background_gradient(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue) {
    map_blend_background_gradient_color(mode, frame_duration, red, green, blue);
}

/* map_blend_ambient_light_color with the color offsets narrowed to s16:
 * the entry event MapDarkness calls. */
void map_start_ambient_light_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue) {
    map_blend_ambient_light_color(mode, frame_duration, red, green, blue);
}

/* map_blend_darkness_color with the color offsets narrowed to s16. The
 * arguments are the mode, the frame duration and the red, green and blue offsets. */
void map_start_darkness_blend(s32 first, s32 second, s16 third, s16 fourth, s16 fifth) {
    map_blend_darkness_color(first, second, third, fourth, fifth);
}

/* map_update_palette_colors for all sixteen CLUTs of the map row. */
void map_update_all_palettes_from_color_data(s32 mode, const clut_t color_data) {
    map_update_palette_colors(mode, MAP_CLUT_ROW_MAP, 0, 1, color_data);
}

/* map_update_palette_colors for one CLUT of the unit row. A clut_index of
 * 16 or more applies the same sixteen colors to every unit CLUT. */
void map_update_palette_colors_by_index(s32 mode, s32 clut_index, const palette_t* color_data) {
    s32 index;

    if (clut_index >= MAP_CLUTS_PER_ROW) {
        for (index = 0; index < MAP_CLUTS_PER_ROW; index++) {
            map_update_palette_colors(mode, MAP_CLUT_ROW_UNITS, index, 0, color_data);
        }
    } else {
        map_update_palette_colors(mode, MAP_CLUT_ROW_UNITS, clut_index, 0, color_data);
    }
}

/* Fade the background gradient to `colors` without changing the base pair, unlike
 * map_transition_and_store_background_gradient. */
void map_set_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors) {
    map_transition_background_gradient(frame_duration, colors);
}

/* Fade the ambient light to `color` without changing the base color, unlike
 * map_transition_and_store_ambient_light_color. */
void map_set_ambient_light_color(s32 frame_duration, const CVECTOR* color) {
    map_transition_ambient_light_color(frame_duration, color);
}

/* Fade the map darkness to `color` without changing the base color, unlike
 * map_transition_and_store_darkness_color. */
void map_set_darkness_color(s32 frame_duration, const CVECTOR* color) {
    map_transition_darkness_color(frame_duration, color);
}
