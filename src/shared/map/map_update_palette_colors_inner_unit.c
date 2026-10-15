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

/* Set or blend CLUTs of a map row to `colors`: all sixteen (256 colors) when
 * update_all is 1, otherwise CLUT clut_index (16 colors). Mode 0 applies the
 * colors at once; another value starts a blend toward them and is kept as the
 * animation's mode (map_update_palette_colors_inner). Marks the rows for
 * upload. */
void map_update_palette_colors(s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors) {
    s32 index;

    if (update_all == 1) {
        const palette_t* current_colors;

        index = 0;
        current_colors = colors;
        while (index < MAP_CLUTS_PER_ROW) {
            map_update_palette_colors_inner(mode, clut_row, index, *current_colors);
            index++;
            current_colors++;
        }
        g_map_palette_state.upload_pending = 1;
    } else {
        map_update_palette_colors_inner(mode, clut_row, clut_index, *colors);
        g_map_palette_state.upload_pending = 1;
    }
}

/* map_update_palette_colors, then copy the colors into the row's packed
 * mirror as well, which makes them the base colors that the MAP_COLOR_MODE_BASE
 * modes and RESTORE of map_modify_palette start from: all 256 when
 * update_all is 1, otherwise the 16 of CLUT clut_index. */
void map_update_and_store_palette_colors(
    s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors) {
    s32 index;
    const palette_t* colors_reg = colors;

    map_update_palette_colors(mode, clut_row, clut_index, update_all, colors_reg);

    /* The packed mirror is copied with native 16-bit loads and stores. */
    if (update_all == 1) {
        u16* destination;
        const u16* source;

        index = 0;
        destination = g_map_palette_state.clut_rows[clut_row].packed_colors;
        source = (const u16*)colors_reg;
        while (index < MAP_CLUT_ROW_COLOR_COUNT) {
            *destination++ = *source++;
            index++;
        }
    } else {
        const u16* source;
        u16* palette_colors;
        s32 palette_offset;

        index = 0;
        palette_offset = clut_index * MAP_CLUT_COLOR_COUNT;
        palette_colors = g_map_palette_state.clut_rows[clut_row].packed_colors;
        source = (const u16*)colors_reg;
        while (index < MAP_CLUT_COLOR_COUNT) {
            palette_colors[palette_offset + index] = *source++;
            index++;
        }
    }
}
