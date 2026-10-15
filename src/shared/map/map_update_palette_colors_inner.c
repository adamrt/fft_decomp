#include "fft/battle_map.h"
#include "psx/types.h"

/* Update one CLUT of a map CLUT row immediately or stage an animation.
 *
 * Mode 0 decodes the ABBBBBGGGGGRRRRR entries into the row's runtime
 * components and rebuilds the upload copy while preserving each stored
 * alpha bit. Other modes store biased component deltas and arm the CLUT's
 * animation state. */
void map_update_palette_colors_inner(s32 mode, s32 clut_row, s32 clut_index, const palette_t colors) {
    s32 index;
    s16 green_for_packing;
    u32 blue_delta;
    u16 red_for_packing;

    index = 0;
    do {
        s32 color_index;
        u16 color;
        map_palette_color_components_t* component;
        u8 green;
        u32 green_value;
        u32 blue_value;
        u8 red;
        u32 red_value;
        u8 blue;

        color_index = clut_index * MAP_CLUT_COLOR_COUNT + index;
        component = &g_map_palette_state.clut_rows[clut_row].components[color_index];
        /* Named component fields reverse the native mask/shift order. */
        color = *(const u16*)colors;
        red_value = color & 0x1f;
        red_for_packing = red_value;
        red = red_for_packing;
        green_value = (color & 0x3e0) >> 5;
        green_for_packing = green_value;
        green = green_for_packing;
        blue_value = (color & 0x7c00) >> 10;
        blue = blue_value;

        if (mode != 0) {
            component->delta_biased.r = red - component->color.r + MAP_PALETTE_DELTA_BIAS;
            blue_delta = blue - component->color.b + MAP_PALETTE_DELTA_BIAS;
            component->delta_biased.g = green - component->color.g + MAP_PALETTE_DELTA_BIAS;
            component->delta_biased.b = blue_delta;
        } else {
            component->color.g = green;
            component->color.r = red;
            component->color.b = blue;
            g_map_palette_modified_colors[clut_row][color_index]
                = red_for_packing + (green_for_packing << 5) + (blue_value << 10) + (component->semi_transparent << 15);
        }

        index++;
        colors++;
    } while (index < MAP_CLUT_COLOR_COUNT);

    if (mode != 0) {
        map_palette_animation_state_t* animation;

        animation = &g_map_palette_state.clut_rows[clut_row].animations[clut_index];
        animation->active = 1;
        animation->blend_step = 0;
        animation->delay_counter = 0;
        animation->mode = mode;
    } else {
        g_map_palette_state.clut_rows[clut_row].animations[clut_index].active = 0;
    }
}
