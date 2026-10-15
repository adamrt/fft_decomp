/* Map palette updates. Its start is an object boundary the read-only data layout places somewhere nearby; its end is a
 * cut chosen by topic. */

#include "fft/battle_map.h"
#include "psx/types.h"

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

void map_store_modified_palette_colors(s32 clut_row, s32 clut_index, s32 store_all) {
    if (store_all == 1) {
        u16* destination;
        u16* source;

        store_all = 0;
        destination = g_map_palette_state.clut_rows[clut_row].packed_colors;
        source = g_map_palette_modified_colors[0];
        source += clut_row * MAP_CLUT_ROW_COLOR_COUNT;
        while (store_all < MAP_CLUT_ROW_COLOR_COUNT) {
            *destination++ = *source++;
            store_all++;
        }
    } else {
        u16* packed_row;

        store_all = 0;
        clut_index *= MAP_CLUT_COLOR_COUNT;
        packed_row = g_map_palette_state.clut_rows[clut_row].packed_colors;
        while (store_all < MAP_CLUT_COLOR_COUNT) {
            packed_row[clut_index + store_all] = g_map_palette_modified_colors[clut_row][clut_index + store_all];
            store_all++;
        }
    }
}

/* Tint a map CLUT row and stage or apply the result.
 *
 * all_colors == 1 rewrites all 256 colors of the row; otherwise only the
 * CLUT `clut_index`. Commands 0-3 start from the decoded components,
 * 4-8 from the packed mirror: 0/4/9 add the bias, 1/5 halve each
 * component first, 2/6 and 3/7 add the bias to a weighted gray
 * ((2R + 3G + B) / 6 or / 12), 8 copies the packed color, and 10 clears the
 * repeat record and returns. Black components are left alone (their deltas
 * become neutral). A nonzero amount stores biased deltas and arms the CLUT
 * animations with that step; zero applies the colors immediately and marks
 * the palette upload pending.
 *
 * The packed-color reads repeat `clut_index * MAP_CLUT_COLOR_COUNT + i` (no index local) and
 * the gray cases use case-local temporaries: both keep the target's loop
 * strength reduction and local register allocation. */
void map_modify_palette(
    s32 command, s32 amount, s32 clut_row, s32 clut_index, s32 all_colors, s16 red, s16 green, s16 blue) {
    s32 i;
    s16 out_red;
    s16 out_green;
    s16 out_blue;
    s32 red_part;
    map_palette_color_components_t* components;
    map_palette_color_components_t* clut;
    map_palette_color_components_t* component;
    map_palette_animation_state_t* animation;

    if (all_colors == 1) {
        components = g_map_palette_state.clut_rows[clut_row].components;
        for (i = 0; i < MAP_CLUT_ROW_COLOR_COUNT; i++) {
            if (components[i].color.r + components[i].color.g + components[i].color.b != 0) {
                switch (command) {
                case MAP_COLOR_MODE_CURRENT:
                    out_red = components[i].color.r + red;
                    out_green = components[i].color.g + green;
                    out_blue = components[i].color.b + blue;
                    break;
                case MAP_COLOR_MODE_CURRENT_HALF:
                    out_red = (components[i].color.r >> 1) + red;
                    out_green = (components[i].color.g >> 1) + green;
                    out_blue = (components[i].color.b >> 1) + blue;
                    break;
                case MAP_COLOR_MODE_CURRENT_GRAY: {
                    s32 gray;
                    gray = (components[i].color.r * 2 + components[i].color.g * 3 + components[i].color.b) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case MAP_COLOR_MODE_CURRENT_GRAY_HALF: {
                    s32 gray;
                    gray = (components[i].color.r * 2 + components[i].color.g * 3 + components[i].color.b) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case MAP_COLOR_MODE_BASE:
                case MAP_COLOR_MODE_BASE_REPEAT:
                    out_red = red + (g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x1f);
                    out_green = green + ((g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x3e0) >> 5);
                    out_blue = blue + ((g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x7c00) >> 10);
                    break;
                case MAP_COLOR_MODE_BASE_HALF:
                    red_part = g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green = green + ((g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x3e0) >> 6);
                    out_blue = blue + ((g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x7c00) >> 11);
                    break;
                case MAP_COLOR_MODE_BASE_GRAY: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits = g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x3e0;
                    blue_bits = g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x7c00;
                    red_bits = g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x1f;
                    gray = red_bits * 2 + (green_bits >> 5) * 3 + (blue_bits >> 10);
                    gray /= 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case MAP_COLOR_MODE_BASE_GRAY_HALF: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits = g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x3e0;
                    blue_bits = g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x7c00;
                    red_bits = g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x1f;
                    gray = red_bits * 2 + (green_bits >> 5) * 3 + (blue_bits >> 10);
                    gray /= 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case MAP_COLOR_MODE_RESTORE:
                    out_red = g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x1f;
                    out_green = (g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x3e0) >> 5;
                    out_blue = (g_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x7c00) >> 10;
                    break;
                case MAP_COLOR_MODE_STOP_REPEAT:
                    for (i = MAP_CLUTS_PER_ROW - 1; i >= 0; i--) {
                        g_map_palette_state.clut_rows[clut_row].animations[i].repeat = MAP_COLOR_REPEAT_OFF;
                    }
                    return;
                }
                if (out_red >= 0x20) {
                    out_red = 0x1f;
                }
                if (out_green >= 0x20) {
                    out_green = 0x1f;
                }
                if (out_blue >= 0x20) {
                    out_blue = 0x1f;
                }
                if (out_red <= 0) {
                    out_red = 0;
                }
                if (out_green <= 0) {
                    out_green = 0;
                }
                if (out_blue <= 0) {
                    out_blue = 0;
                }
                if ((out_red | out_green | out_blue) == 0) {
                    out_blue = 1;
                }
                if (amount != 0) {
                    components[i].delta_biased.r = out_red - components[i].color.r + MAP_PALETTE_DELTA_BIAS;
                    components[i].delta_biased.g = out_green - components[i].color.g + MAP_PALETTE_DELTA_BIAS;
                    components[i].delta_biased.b = out_blue - components[i].color.b + MAP_PALETTE_DELTA_BIAS;
                } else {
                    components[i].color.r = out_red;
                    components[i].color.g = out_green;
                    components[i].color.b = out_blue;
                    g_map_palette_modified_colors[clut_row][i]
                        = out_red + (out_green << 5) + (out_blue << 10) + (components[i].semi_transparent << 15);
                }
            } else {
                components[i].delta_biased.r = components[i].delta_biased.g = components[i].delta_biased.b
                    = MAP_PALETTE_DELTA_BIAS;
            }
        }
        if (amount != 0) {
            for (i = 0; i < MAP_CLUTS_PER_ROW; i++) {
                animation = &g_map_palette_state.clut_rows[clut_row].animations[i];
                animation->active = 1;
                animation->blend_step = 0;
                animation->delay_counter = 0;
                animation->mode = amount;
                if (command == MAP_COLOR_MODE_BASE_REPEAT) {
                    animation->repeat = MAP_COLOR_REPEAT_TINTED;
                    animation->repeat_color.r = red;
                    animation->repeat_color.g = green;
                    animation->repeat_color.b = blue;
                }
            }
            return;
        }
        for (i = MAP_CLUTS_PER_ROW - 1; i >= 0; i--) {
            g_map_palette_state.clut_rows[clut_row].animations[i].active = 0;
        }
        g_map_palette_state.upload_pending = 1;
    } else {
        clut = &g_map_palette_state.clut_rows[clut_row].components[clut_index * MAP_CLUT_COLOR_COUNT];
        for (i = 0; i < MAP_CLUT_COLOR_COUNT; i++) {
            component = &clut[i];
            if (component->color.r + component->color.g + component->color.b != 0) {
                switch (command) {
                case MAP_COLOR_MODE_CURRENT:
                    out_red = component->color.r + red;
                    out_green = component->color.g + green;
                    out_blue = component->color.b + blue;
                    break;
                case MAP_COLOR_MODE_CURRENT_HALF:
                    out_red = (component->color.r >> 1) + red;
                    out_green = (component->color.g >> 1) + green;
                    out_blue = (component->color.b >> 1) + blue;
                    break;
                case MAP_COLOR_MODE_CURRENT_GRAY: {
                    s32 gray;
                    gray = (component->color.r * 2 + component->color.g * 3 + component->color.b) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case MAP_COLOR_MODE_CURRENT_GRAY_HALF: {
                    s32 gray;
                    gray = (component->color.r * 2 + component->color.g * 3 + component->color.b) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case MAP_COLOR_MODE_BASE:
                case MAP_COLOR_MODE_BASE_REPEAT:
                    out_red = red
                        + (g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                            & 0x1f);
                    out_green = green
                        + ((g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                               & 0x3e0)
                            >> 5);
                    out_blue = blue
                        + ((g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                               & 0x7c00)
                            >> 10);
                    break;
                case MAP_COLOR_MODE_BASE_HALF:
                    red_part
                        = g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                        & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green = green
                        + ((g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                               & 0x3e0)
                            >> 6);
                    out_blue = blue
                        + ((g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                               & 0x7c00)
                            >> 11);
                    break;
                case MAP_COLOR_MODE_BASE_GRAY: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits
                        = g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                        & 0x3e0;
                    blue_bits
                        = g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                        & 0x7c00;
                    red_bits
                        = g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                        & 0x1f;
                    gray = red_bits * 2 + (green_bits >> 5) * 3 + (blue_bits >> 10);
                    gray /= 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case MAP_COLOR_MODE_BASE_GRAY_HALF: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits
                        = g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                        & 0x3e0;
                    blue_bits
                        = g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                        & 0x7c00;
                    red_bits
                        = g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                        & 0x1f;
                    gray = red_bits * 2 + (green_bits >> 5) * 3 + (blue_bits >> 10);
                    gray /= 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case MAP_COLOR_MODE_RESTORE:
                    out_red
                        = g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                        & 0x1f;
                    out_green
                        = (g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                              & 0x3e0)
                        >> 5;
                    out_blue
                        = (g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                              & 0x7c00)
                        >> 10;
                    break;
                case MAP_COLOR_MODE_STOP_REPEAT:
                    g_map_palette_state.clut_rows[clut_row].animations[clut_index].repeat = MAP_COLOR_REPEAT_OFF;
                    return;
                }
                if (out_red >= 0x20) {
                    out_red = 0x1f;
                }
                if (out_green >= 0x20) {
                    out_green = 0x1f;
                }
                if (out_blue >= 0x20) {
                    out_blue = 0x1f;
                }
                if (out_red <= 0) {
                    out_red = 0;
                }
                if (out_green <= 0) {
                    out_green = 0;
                }
                if (out_blue <= 0) {
                    out_blue = 0;
                }
                if ((out_red | out_green | out_blue) == 0) {
                    out_blue = 1;
                }
                if (amount != 0) {
                    component->delta_biased.r = out_red - component->color.r + MAP_PALETTE_DELTA_BIAS;
                    component->delta_biased.g = out_green - component->color.g + MAP_PALETTE_DELTA_BIAS;
                    component->delta_biased.b = out_blue - component->color.b + MAP_PALETTE_DELTA_BIAS;
                } else {
                    component->color.r = out_red;
                    component->color.g = out_green;
                    component->color.b = out_blue;
                    g_map_palette_modified_colors[clut_row][clut_index * MAP_CLUT_COLOR_COUNT + i]
                        = out_red + (out_green << 5) + (out_blue << 10) + (component->semi_transparent << 15);
                }
            } else {
                component->delta_biased.r = component->delta_biased.g = component->delta_biased.b
                    = MAP_PALETTE_DELTA_BIAS;
            }
        }
        if (amount != 0) {
            animation = &g_map_palette_state.clut_rows[clut_row].animations[clut_index];
            animation->active = 1;
            animation->blend_step = 0;
            animation->delay_counter = 0;
            animation->mode = amount;
            if (command == MAP_COLOR_MODE_BASE_REPEAT) {
                animation->repeat = MAP_COLOR_REPEAT_TINTED;
                animation->repeat_color.r = red;
                animation->repeat_color.g = green;
                animation->repeat_color.b = blue;
            }
            return;
        }
        g_map_palette_state.clut_rows[clut_row].animations[clut_index].active = 0;
        g_map_palette_state.upload_pending = 1;
    }
}

/*
 * Starts a fade of both background gradient colors toward `colors` over
 * frame_duration * 8 frames, or applies them at once when frame_duration is
 * zero. Indexing base[i]/color[i] (not advancing pointers) is required: a
 * pointer walk splits into biased givs.
 */
void map_transition_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors) {
    map_color_transition_channels_t* base;
    const CVECTOR* color;
    map_background_gradient_colors_t current;
    s32 i;
    s32 red;
    s32 green;
    s32 blue;
    s32 target_red;
    s32 target_green;
    s32 target_blue;

    base = g_map_background_gradient_transition.channels;
    color = colors->colors;
    for (i = 0; i < 2; i++) {
        red = color[i].r << 16;
        target_red = red;
        green = color[i].g << 16;
        target_green = green;
        blue = color[i].b << 16;
        target_blue = blue;
        if (frame_duration != 0) {
            base[i].red_step = (red - (base[i].red & ~0xFFFF)) / (frame_duration * 8);
            base[i].green_step = (green - (base[i].green & ~0xFFFF)) / (frame_duration * 8);
            base[i].blue_step = (blue - (base[i].blue & ~0xFFFF)) / (frame_duration * 8);
        } else {
            base[i].red = target_red;
            base[i].green = target_green;
            base[i].blue = target_blue;
        }
    }
    if (frame_duration != 0) {
        g_map_background_gradient_transition.active = 1;
        g_map_background_gradient_transition.phase = 0;
        g_map_background_gradient_transition.tick = 0;
        g_map_background_gradient_transition.period = frame_duration;
    } else {
        g_map_background_gradient_transition.active = 0;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].r = base[0].red / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].g = base[0].green / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].b = base[0].blue / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r = base[1].red / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g = base[1].green / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b = base[1].blue / 0x10000;
        map_light_state_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &current);
    }
}

/* Fade the background gradient to `colors` and make them the base pair that the
 * MAP_COLOR_MODE_BASE modes and RESTORE start from. */
void map_transition_and_store_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors) {
    map_transition_background_gradient(frame_duration, colors);
    g_map_background_gradient_transition.colors[0] = colors->colors[MAP_BACKGROUND_GRADIENT_TOP];
    g_map_background_gradient_transition.colors[1] = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
}
