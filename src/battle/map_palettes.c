/* Map palettes, including background gradient, ambient light and darkness transitions, CLUT cycles, VRAM palette
 * uploads, unit palette modulation, and the deployment frame loop. */

#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_map_update_palette_colors_inner(s32 mode, s32 clut_row, s32 clut_index, const palette_t colors) {
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
void battle_map_update_palette_colors(s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors) {
    s32 index;

    if (update_all == 1) {
        const palette_t* current_colors;

        index = 0;
        current_colors = colors;
        while (index < MAP_CLUTS_PER_ROW) {
            battle_map_update_palette_colors_inner(mode, clut_row, index, *current_colors);
            index++;
            current_colors++;
        }
        g_map_palette_state.upload_pending = 1;
    } else {
        battle_map_update_palette_colors_inner(mode, clut_row, clut_index, *colors);
        g_map_palette_state.upload_pending = 1;
    }
}

/* map_update_palette_colors, then copy the colors into the row's packed
 * mirror as well, which makes them the base colors that the MAP_COLOR_MODE_BASE
 * modes and RESTORE of map_modify_palette start from: all 256 when
 * update_all is 1, otherwise the 16 of CLUT clut_index. */
void battle_map_update_and_store_palette_colors(
    s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors) {
    s32 index;
    const palette_t* colors_reg = colors;

    battle_map_update_palette_colors(mode, clut_row, clut_index, update_all, colors_reg);

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

void battle_map_store_modified_palette_colors(s32 clut_row, s32 clut_index, s32 store_all) {
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
void battle_map_modify_palette(
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
void battle_map_transition_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors) {
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
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &current);
    }
}

/* Fade the background gradient to `colors` and make them the base pair that the
 * MAP_COLOR_MODE_BASE modes and RESTORE start from. */
void battle_map_transition_and_store_background_gradient(
    s32 frame_duration, const map_background_gradient_colors_t* colors) {
    battle_map_transition_background_gradient(frame_duration, colors);
    g_map_background_gradient_transition.colors[0] = colors->colors[MAP_BACKGROUND_GRADIENT_TOP];
    g_map_background_gradient_transition.colors[1] = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
}

void battle_map_blend_background_gradient_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue) {
    map_color_transition_channels_t* channels;
    map_background_gradient_colors_t current;
    s32 target_red;
    s32 target_green;
    s32 target_blue;
    s32 base_red;
    s32 base_green;
    s32 base_blue;
    s32 i;

    channels = g_map_background_gradient_transition.channels;
    for (i = 0; i < 2; i++) {
        switch (mode) {
        case MAP_COLOR_MODE_CURRENT:
            target_red = (red << 16) + channels[i].red;
            target_green = (green << 16) + channels[i].green;
            target_blue = (blue << 16) + channels[i].blue;
            break;
        case MAP_COLOR_MODE_CURRENT_HALF:
            target_red = channels[i].red / 2 + (red << 16);
            target_green = channels[i].green / 2 + (green << 16);
            target_blue = channels[i].blue / 2 + (blue << 16);
            break;
        case MAP_COLOR_MODE_CURRENT_GRAY: {
            s32 gray = (channels[i].red * 2 + channels[i].green * 3 + channels[i].blue) / 6;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case MAP_COLOR_MODE_CURRENT_GRAY_HALF: {
            s32 gray = (channels[i].red * 2 + channels[i].green * 3 + channels[i].blue) / 12;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case MAP_COLOR_MODE_BASE:
        case MAP_COLOR_MODE_BASE_REPEAT:
            target_red = (g_map_background_gradient_transition.colors[i].r + red) << 16;
            target_green = (g_map_background_gradient_transition.colors[i].g + green) << 16;
            target_blue = (g_map_background_gradient_transition.colors[i].b + blue) << 16;
            break;
        case MAP_COLOR_MODE_BASE_HALF:
            target_red = (g_map_background_gradient_transition.colors[i].r << 15) + (red << 16);
            target_green = (g_map_background_gradient_transition.colors[i].g << 15) + (green << 16);
            target_blue = (g_map_background_gradient_transition.colors[i].b << 15) + (blue << 16);
            break;
        case MAP_COLOR_MODE_BASE_GRAY: {
            s32 gray;

            base_red = g_map_background_gradient_transition.colors[i].r << 16;
            base_green = g_map_background_gradient_transition.colors[i].g << 16;
            base_blue = g_map_background_gradient_transition.colors[i].b << 16;
            gray = (base_red * 2 + base_green * 3 + base_blue) / 6;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case MAP_COLOR_MODE_BASE_GRAY_HALF: {
            s32 gray;

            base_red = g_map_background_gradient_transition.colors[i].r << 16;
            base_green = g_map_background_gradient_transition.colors[i].g << 16;
            base_blue = g_map_background_gradient_transition.colors[i].b << 16;
            gray = (base_red * 2 + base_green * 3 + base_blue) / 12;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case MAP_COLOR_MODE_RESTORE:
            target_red = g_map_background_gradient_transition.colors[i].r << 16;
            target_green = g_map_background_gradient_transition.colors[i].g << 16;
            target_blue = g_map_background_gradient_transition.colors[i].b << 16;
            break;
        case MAP_COLOR_MODE_STOP_REPEAT:
            g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_OFF;
            return;
        }

        target_red &= ~0xFFFF;
        target_green &= ~0xFFFF;
        target_blue &= ~0xFFFF;
        if (target_red > 0xFF0000) {
            target_red = 0xFF0000;
        }
        if (target_green > 0xFF0000) {
            target_green = 0xFF0000;
        }
        if (target_blue > 0xFF0000) {
            target_blue = 0xFF0000;
        }
        if (target_red < 0) {
            target_red = 0;
        }
        if (target_green < 0) {
            target_green = 0;
        }
        if (target_blue < 0) {
            target_blue = 0;
        }

        if (frame_duration != 0) {
            channels[i].red_step = (target_red - (channels[i].red & ~0xFFFF)) / (frame_duration * 8);
            channels[i].green_step = (target_green - (channels[i].green & ~0xFFFF)) / (frame_duration * 8);
            channels[i].blue_step = (target_blue - (channels[i].blue & ~0xFFFF)) / (frame_duration * 8);
        } else {
            channels[i].red = target_red;
            channels[i].green = target_green;
            channels[i].blue = target_blue;
        }
    }
    if (frame_duration != 0) {
        g_map_background_gradient_transition.active = 1;
        g_map_background_gradient_transition.phase = 0;
        g_map_background_gradient_transition.tick = 0;
        g_map_background_gradient_transition.period = frame_duration;
        if (mode == MAP_COLOR_MODE_BASE_REPEAT) {
            g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_TINTED;
            g_map_background_gradient_transition.repeat_color.r = red;
            g_map_background_gradient_transition.repeat_color.g = green;
            g_map_background_gradient_transition.repeat_color.b = blue;
        }
    } else {
        g_map_background_gradient_transition.active = 0;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].r = channels[0].red / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].g = channels[0].green / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_TOP].b = channels[0].blue / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r = channels[1].red / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g = channels[1].green / 0x10000;
        current.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b = channels[1].blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &current);
    }
}

/* Start a fade of the ambient light toward color over frame_duration * 8
 * frames, or apply it at once when frame_duration is zero. */
void battle_map_transition_ambient_light_color(s32 frame_duration, const CVECTOR* color) {
    map_color_transition_channels_t* channels;
    CVECTOR current;
    s32 red;
    s32 green;
    s32 blue;
    s32 target_red;
    s32 target_green;
    s32 target_blue;

    channels = &g_map_ambient_light_transition.channels;
    /* The separate target copies reproduce the target's register copies that
     * feed the immediate-store path. */
    red = color->r << 16;
    target_red = red;
    green = color->g << 16;
    target_green = green;
    blue = color->b << 16;
    target_blue = blue;
    if (frame_duration != 0) {
        g_map_ambient_light_transition.channels.red_step = (red - (channels->red & ~0xFFFF)) / (frame_duration * 8);
        g_map_ambient_light_transition.channels.green_step
            = (green - (channels->green & ~0xFFFF)) / (frame_duration * 8);
        g_map_ambient_light_transition.channels.blue_step = (blue - (channels->blue & ~0xFFFF)) / (frame_duration * 8);
    } else {
        channels->red = target_red;
        channels->green = target_green;
        channels->blue = target_blue;
    }
    if (frame_duration != 0) {
        g_map_ambient_light_transition.active = 1;
        g_map_ambient_light_transition.phase = 0;
        g_map_ambient_light_transition.tick = 0;
        g_map_ambient_light_transition.period = frame_duration;
    } else {
        g_map_ambient_light_transition.active = 0;
        current.r = channels->red / 0x10000;
        current.g = channels->green / 0x10000;
        current.b = channels->blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, &current);
    }
}

/* Fade the ambient light to `color` and make it the base color that the
 * MAP_COLOR_MODE_BASE modes and RESTORE start from. */
void battle_map_transition_and_store_ambient_light_color(s32 frame_duration, const CVECTOR* color) {
    battle_map_transition_ambient_light_color(frame_duration, color);
    g_map_ambient_light_color = *color;
}

void battle_map_blend_ambient_light_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue) {
    map_color_transition_channels_t* channels;
    CVECTOR current;
    s32 target_red;
    s32 target_green;
    s32 target_blue;
    s32 gray;
    s32 base_red;
    s32 base_green;
    s32 base_blue;

    channels = &g_map_ambient_light_transition.channels;
    switch (mode) {
    case MAP_COLOR_MODE_CURRENT:
        target_red = (red << 16) + channels->red;
        target_green = (green << 16) + channels->green;
        target_blue = (blue << 16) + channels->blue;
        break;
    case MAP_COLOR_MODE_CURRENT_HALF:
        target_red = channels->red / 2 + (red << 16);
        target_green = channels->green / 2 + (green << 16);
        target_blue = channels->blue / 2 + (blue << 16);
        break;
    case MAP_COLOR_MODE_CURRENT_GRAY:
        gray = (channels->red * 2 + channels->green * 3 + channels->blue) / 6;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_CURRENT_GRAY_HALF:
        gray = (channels->red * 2 + channels->green * 3 + channels->blue) / 12;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_BASE:
    case MAP_COLOR_MODE_BASE_REPEAT:
        target_red = (g_map_ambient_light_color.r + red) << 16;
        target_green = (g_map_ambient_light_color.g + green) << 16;
        target_blue = (g_map_ambient_light_color.b + blue) << 16;
        break;
    case MAP_COLOR_MODE_BASE_HALF:
        /* The base color is unsigned, so the halved 16.16 value is a shift. */
        target_red = (g_map_ambient_light_color.r << 15) + (red << 16);
        target_green = (g_map_ambient_light_color.g << 15) + (green << 16);
        target_blue = (g_map_ambient_light_color.b << 15) + (blue << 16);
        break;
    case MAP_COLOR_MODE_BASE_GRAY:
        base_red = g_map_ambient_light_color.r << 16;
        base_green = g_map_ambient_light_color.g << 16;
        base_blue = g_map_ambient_light_color.b << 16;
        gray = (base_red * 2 + base_green * 3 + base_blue) / 6;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_BASE_GRAY_HALF:
        base_red = g_map_ambient_light_color.r << 16;
        base_green = g_map_ambient_light_color.g << 16;
        base_blue = g_map_ambient_light_color.b << 16;
        gray = (base_red * 2 + base_green * 3 + base_blue) / 12;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_RESTORE:
        target_red = g_map_ambient_light_color.r << 16;
        target_green = g_map_ambient_light_color.g << 16;
        target_blue = g_map_ambient_light_color.b << 16;
        break;
    case MAP_COLOR_MODE_STOP_REPEAT:
        g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_OFF;
        return;
    }

    target_red &= ~0xFFFF;
    target_green &= ~0xFFFF;
    target_blue &= ~0xFFFF;
    if (target_red > 0xFF0000) {
        target_red = 0xFF0000;
    }
    if (target_green > 0xFF0000) {
        target_green = 0xFF0000;
    }
    if (target_blue > 0xFF0000) {
        target_blue = 0xFF0000;
    }
    if (target_red < 0) {
        target_red = 0;
    }
    if (target_green < 0) {
        target_green = 0;
    }
    if (target_blue < 0) {
        target_blue = 0;
    }

    if (frame_duration != 0) {
        channels->red_step = (target_red - (channels->red & ~0xFFFF)) / (frame_duration * 8);
        channels->green_step = (target_green - (channels->green & ~0xFFFF)) / (frame_duration * 8);
        channels->blue_step = (target_blue - (channels->blue & ~0xFFFF)) / (frame_duration * 8);
    } else {
        channels->red = target_red;
        channels->green = target_green;
        channels->blue = target_blue;
    }
    if (frame_duration != 0) {
        g_map_ambient_light_transition.active = 1;
        g_map_ambient_light_transition.phase = 0;
        g_map_ambient_light_transition.tick = 0;
        g_map_ambient_light_transition.period = frame_duration;
        if (mode == MAP_COLOR_MODE_BASE_REPEAT) {
            g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_TINTED;
            g_map_ambient_light_transition.repeat_color.r = red;
            g_map_ambient_light_transition.repeat_color.g = green;
            g_map_ambient_light_transition.repeat_color.b = blue;
        }
    } else {
        g_map_ambient_light_transition.active = 0;
        current.r = channels->red / 0x10000;
        current.g = channels->green / 0x10000;
        current.b = channels->blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, &current);
    }
}

/* Start a fade of the map darkness toward color over frame_duration * 8
 * frames, or apply it at once when frame_duration is zero. */
void battle_map_transition_darkness_color(s32 frame_duration, const CVECTOR* color) {
    map_color_transition_channels_t* channels;
    CVECTOR current;
    s32 red;
    s32 green;
    s32 blue;
    s32 target_red;
    s32 target_green;
    s32 target_blue;

    channels = &g_map_darkness_transition.channels;
    /* The separate target copies reproduce the target's register copies that
     * feed the immediate-store path. */
    red = color->r << 16;
    target_red = red;
    green = color->g << 16;
    target_green = green;
    blue = color->b << 16;
    target_blue = blue;
    if (frame_duration != 0) {
        g_map_darkness_transition.channels.red_step = (red - (channels->red & ~0xFFFF)) / (frame_duration * 8);
        g_map_darkness_transition.channels.green_step = (green - (channels->green & ~0xFFFF)) / (frame_duration * 8);
        g_map_darkness_transition.channels.blue_step = (blue - (channels->blue & ~0xFFFF)) / (frame_duration * 8);
    } else {
        channels->red = target_red;
        channels->green = target_green;
        channels->blue = target_blue;
    }
    if (frame_duration != 0) {
        g_map_darkness_transition.active = 1;
        g_map_darkness_transition.phase = 0;
        g_map_darkness_transition.tick = 0;
        g_map_darkness_transition.period = frame_duration;
    } else {
        g_map_darkness_transition.active = 0;
        current.r = channels->red / 0x10000;
        current.g = channels->green / 0x10000;
        current.b = channels->blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &current);
    }
}

/* Fade the map darkness to `color` and make it the base color that the
 * MAP_COLOR_MODE_BASE modes and RESTORE start from. */
void battle_map_transition_and_store_darkness_color(s32 frame_duration, const CVECTOR* color) {
    battle_map_transition_darkness_color(frame_duration, color);
    g_map_darkness_color = *color;
}

void battle_map_blend_darkness_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue) {
    map_color_transition_channels_t* channels;
    CVECTOR current;
    s32 target_red;
    s32 target_green;
    s32 target_blue;
    s32 gray;
    s32 base_red;
    s32 base_green;
    s32 base_blue;

    channels = &g_map_darkness_transition.channels;
    switch (mode) {
    case MAP_COLOR_MODE_CURRENT:
        target_red = (red << 16) + channels->red;
        target_green = (green << 16) + channels->green;
        target_blue = (blue << 16) + channels->blue;
        break;
    case MAP_COLOR_MODE_CURRENT_HALF:
        target_red = channels->red / 2 + (red << 16);
        target_green = channels->green / 2 + (green << 16);
        target_blue = channels->blue / 2 + (blue << 16);
        break;
    case MAP_COLOR_MODE_CURRENT_GRAY:
        gray = (channels->red * 2 + channels->green * 3 + channels->blue) / 6;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_CURRENT_GRAY_HALF:
        gray = (channels->red * 2 + channels->green * 3 + channels->blue) / 12;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_BASE:
    case MAP_COLOR_MODE_BASE_REPEAT:
        target_red = (g_map_darkness_color.r + red) << 16;
        target_green = (g_map_darkness_color.g + green) << 16;
        target_blue = (g_map_darkness_color.b + blue) << 16;
        break;
    case MAP_COLOR_MODE_BASE_HALF:
        /* The base color is unsigned, so the halved 16.16 value is a shift. */
        target_red = (g_map_darkness_color.r << 15) + (red << 16);
        target_green = (g_map_darkness_color.g << 15) + (green << 16);
        target_blue = (g_map_darkness_color.b << 15) + (blue << 16);
        break;
    case MAP_COLOR_MODE_BASE_GRAY:
        base_red = g_map_darkness_color.r << 16;
        base_green = g_map_darkness_color.g << 16;
        base_blue = g_map_darkness_color.b << 16;
        gray = (base_red * 2 + base_green * 3 + base_blue) / 6;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_BASE_GRAY_HALF:
        base_red = g_map_darkness_color.r << 16;
        base_green = g_map_darkness_color.g << 16;
        base_blue = g_map_darkness_color.b << 16;
        gray = (base_red * 2 + base_green * 3 + base_blue) / 12;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case MAP_COLOR_MODE_RESTORE:
        target_red = g_map_darkness_color.r << 16;
        target_green = g_map_darkness_color.g << 16;
        target_blue = g_map_darkness_color.b << 16;
        break;
    case MAP_COLOR_MODE_STOP_REPEAT:
        g_map_darkness_transition.repeat = MAP_COLOR_REPEAT_OFF;
        return;
    }

    target_red &= ~0xFFFF;
    target_green &= ~0xFFFF;
    target_blue &= ~0xFFFF;
    if (target_red > 0xFF0000) {
        target_red = 0xFF0000;
    }
    if (target_green > 0xFF0000) {
        target_green = 0xFF0000;
    }
    if (target_blue > 0xFF0000) {
        target_blue = 0xFF0000;
    }
    if (target_red < 0) {
        target_red = 0;
    }
    if (target_green < 0) {
        target_green = 0;
    }
    if (target_blue < 0) {
        target_blue = 0;
    }

    if (frame_duration != 0) {
        channels->red_step = (target_red - (channels->red & ~0xFFFF)) / (frame_duration * 8);
        channels->green_step = (target_green - (channels->green & ~0xFFFF)) / (frame_duration * 8);
        channels->blue_step = (target_blue - (channels->blue & ~0xFFFF)) / (frame_duration * 8);
    } else {
        channels->red = target_red;
        channels->green = target_green;
        channels->blue = target_blue;
    }
    if (frame_duration != 0) {
        g_map_darkness_transition.active = 1;
        g_map_darkness_transition.phase = 0;
        g_map_darkness_transition.tick = 0;
        g_map_darkness_transition.period = frame_duration;
        if (mode == MAP_COLOR_MODE_BASE_REPEAT) {
            g_map_darkness_transition.repeat = MAP_COLOR_REPEAT_TINTED;
            g_map_darkness_transition.repeat_color.r = red;
            g_map_darkness_transition.repeat_color.g = green;
            g_map_darkness_transition.repeat_color.b = blue;
        }
    } else {
        g_map_darkness_transition.active = 0;
        current.r = channels->red / 0x10000;
        current.g = channels->green / 0x10000;
        current.b = channels->blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &current);
    }
}

/* Claim a free color rotation slot: the colors first_color..last_color of a
 * CLUT row rotate one place every frame_duration frames. Returns the slot index +
 * 1 as the handle, or 0 when all eight slots are in use. */
s32 battle_map_start_clut_cycle(s32 frame_duration, s32 clut_row, s32 first_color, s32 last_color) {
    s32 index;
    map_clut_cycle_state_t* state;

    for (index = 0; index < MAP_CLUT_CYCLE_SLOT_COUNT; index++) {
        state = &g_map_clut_cycle_states[index];
        if (state->active == 0) {
            state->active = 1;
            state->clut_row = clut_row;
            state->first_color = first_color;
            state->last_color = last_color;
            state->frame_duration = frame_duration;
            return index + 1;
        }
    }
    return 0;
}

/* Stop a color rotation by the handle map_start_clut_cycle
 * returned (slot index + 1). Returns 1 for a handle 1-8, else 0.
 *
 * Retail clears the active byte at handle * 10, not (handle - 1) * 10: it stops
 * the slot after the handle's own, and handle 8 clears the byte that follows the
 * eight slots, the first of g_battle_gfx_cycled_deployed_palettes. */
s32 battle_map_stop_clut_cycle(s32 handle) {
    if (handle >= 1 && handle <= MAP_CLUT_CYCLE_SLOT_COUNT) {
        g_map_clut_cycle_states[handle].active = 0;
        return 1;
    }
    return 0;
}

/* Free the eight color rotation slots (map_clut_cycle_state_t) by
 * clearing their active bytes. The 32 texture animation rows of the map are not
 * touched. The byte-offset loop preserves the original address sequence;
 * indexed active fields add four bytes. */
void battle_map_reset_clut_cycles(void) {
    s32 offset;

    offset = (MAP_CLUT_CYCLE_SLOT_COUNT - 1) * MAP_CLUT_CYCLE_SLOT_BYTE_COUNT;
    do {
        ((u8*)g_map_clut_cycle_states)[offset] = 0;
        offset -= MAP_CLUT_CYCLE_SLOT_BYTE_COUNT;
    } while (offset >= 0);
}

/* Advances the per-frame map animations: texture palette rotations, the
 * background gradient, ambient light and darkness transitions, and the
 * color blends of each map CLUT row.
 *
 * red/green/blue are s16: the extensions of their u8 + s8 sums fold into
 * copies, which reproduces the target's separate stored and packed values
 * and the reloads of the component bytes. The components are indexed from a
 * pointer to the CLUT's first color so loop.c reduces one address giv, as in the target. */
void battle_map_update_animations(void) {
    map_palette_color_components_t saved_component;
    map_background_gradient_colors_t gradient;
    map_clut_cycle_state_t* state;
    map_clut_row_state_t* row_state;
    map_palette_color_components_t* component;
    s32 i;
    s32 j;
    s32 k;
    u16 last;
    s16 red;
    s16 green;
    s16 blue;
    u16 saved_packed;
    u16 saved_modified;
    map_color_transition_t* transition;
    s32 step;

    for (i = 0; i < MAP_CLUT_CYCLE_SLOT_COUNT; i++) {
        state = &g_map_clut_cycle_states[i];
        if (state->active != 0) {
            state->elapsed_frames++;
            if (state->elapsed_frames >= state->frame_duration) {
                state->elapsed_frames = 0;
                row_state = &g_map_palette_state.clut_rows[state->clut_row];
                saved_component = row_state->components[state->last_color];
                last = state->last_color;
                saved_packed = row_state->packed_colors[last];
                saved_modified = g_map_palette_modified_colors[state->clut_row][last];
                if (state->first_color < last) {
                    for (k = last; state->first_color < k; k--) {
                        row_state->components[k] = row_state->components[k - 1];
                        row_state->packed_colors[k] = row_state->packed_colors[k - 1];
                        g_map_palette_modified_colors[state->clut_row][k]
                            = g_map_palette_modified_colors[state->clut_row][k - 1];
                    }
                } else {
                    for (k = last; k < state->first_color; k++) {
                        row_state->components[k] = row_state->components[k + 1];
                        row_state->packed_colors[k] = row_state->packed_colors[k + 1];
                        g_map_palette_modified_colors[state->clut_row][k]
                            = g_map_palette_modified_colors[state->clut_row][k + 1];
                    }
                }
                row_state->components[state->first_color] = saved_component;
                row_state->packed_colors[state->first_color] = saved_packed;
                g_map_palette_modified_colors[state->clut_row][state->first_color] = saved_modified;
                g_map_palette_state.upload_pending = 1;
            }
        }
    }

    if (g_map_background_gradient_transition.active != 0) {
        map_color_transition_channels_t* channels;

        channels = g_map_background_gradient_transition.channels;
        for (i = 0; i < 2; i++) {
            channels[i].red += channels[i].red_step;
            channels[i].green += channels[i].green_step;
            channels[i].blue += channels[i].blue_step;
        }
        gradient.colors[MAP_BACKGROUND_GRADIENT_TOP].r = channels[0].red / 0x10000;
        gradient.colors[MAP_BACKGROUND_GRADIENT_TOP].g = channels[0].green / 0x10000;
        gradient.colors[MAP_BACKGROUND_GRADIENT_TOP].b = channels[0].blue / 0x10000;
        gradient.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r = channels[1].red / 0x10000;
        gradient.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g = channels[1].green / 0x10000;
        gradient.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b = channels[1].blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &gradient);
        if (g_map_background_gradient_transition.period >= 4) {
            if (++g_map_background_gradient_transition.tick >= g_map_background_gradient_transition.period >> 2) {
                g_map_background_gradient_transition.tick = 0;
                if (++g_map_background_gradient_transition.phase >= 0x20) {
                    if (g_map_background_gradient_transition.repeat == MAP_COLOR_REPEAT_TINTED) {
                        battle_map_blend_background_gradient_color(
                            MAP_COLOR_MODE_RESTORE, g_map_background_gradient_transition.period, 0, 0, 0);
                        g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_map_background_gradient_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                        battle_map_blend_background_gradient_color(MAP_COLOR_MODE_BASE_REPEAT,
                            g_map_background_gradient_transition.period,
                            g_map_background_gradient_transition.repeat_color.r,
                            g_map_background_gradient_transition.repeat_color.g,
                            g_map_background_gradient_transition.repeat_color.b);
                        g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_TINTED;
                    } else {
                        g_map_background_gradient_transition.active = 0;
                    }
                }
            }
        } else if (++g_map_background_gradient_transition.phase >= g_map_background_gradient_transition.period * 8) {
            if (g_map_background_gradient_transition.repeat == MAP_COLOR_REPEAT_TINTED) {
                battle_map_blend_background_gradient_color(
                    MAP_COLOR_MODE_RESTORE, g_map_background_gradient_transition.period, 0, 0, 0);
                g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
            } else if (g_map_background_gradient_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                battle_map_blend_background_gradient_color(MAP_COLOR_MODE_BASE_REPEAT,
                    g_map_background_gradient_transition.period, g_map_background_gradient_transition.repeat_color.r,
                    g_map_background_gradient_transition.repeat_color.g,
                    g_map_background_gradient_transition.repeat_color.b);
                g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_TINTED;
            } else {
                g_map_background_gradient_transition.active = 0;
            }
        }
    }

    transition = &g_map_ambient_light_transition;
    /* Barrier: hides the constant so active and the red channel are accessed through s0. */
    __asm__("" : "=r"(transition) : "0"(transition));
    if (transition->active != 0) {
        s32 red_level;

        red_level = transition->channels.red += g_map_ambient_light_transition.channels.red_step;
        g_map_ambient_light_transition.channels.green += g_map_ambient_light_transition.channels.green_step;
        g_map_ambient_light_transition.channels.blue += g_map_ambient_light_transition.channels.blue_step;
        gradient.colors[MAP_BACKGROUND_GRADIENT_TOP].r = red_level / 0x10000;
        gradient.colors[MAP_BACKGROUND_GRADIENT_TOP].g = g_map_ambient_light_transition.channels.green / 0x10000;
        gradient.colors[MAP_BACKGROUND_GRADIENT_TOP].b = g_map_ambient_light_transition.channels.blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, &gradient);
        if (g_map_ambient_light_transition.period >= 4) {
            if (++g_map_ambient_light_transition.tick >= g_map_ambient_light_transition.period >> 2) {
                g_map_ambient_light_transition.tick = 0;
                if (++g_map_ambient_light_transition.phase >= 0x20) {
                    if (g_map_ambient_light_transition.repeat == MAP_COLOR_REPEAT_TINTED) {
                        battle_map_blend_ambient_light_color(
                            MAP_COLOR_MODE_RESTORE, g_map_ambient_light_transition.period, 0, 0, 0);
                        g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_map_ambient_light_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                        battle_map_blend_ambient_light_color(MAP_COLOR_MODE_BASE_REPEAT,
                            g_map_ambient_light_transition.period, g_map_ambient_light_transition.repeat_color.r,
                            g_map_ambient_light_transition.repeat_color.g,
                            g_map_ambient_light_transition.repeat_color.b);
                        g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_TINTED;
                    } else {
                        transition->active = 0;
                    }
                }
            }
        } else if (++g_map_ambient_light_transition.phase >= g_map_ambient_light_transition.period * 8) {
            if (g_map_ambient_light_transition.repeat == MAP_COLOR_REPEAT_TINTED) {
                battle_map_blend_ambient_light_color(
                    MAP_COLOR_MODE_RESTORE, g_map_ambient_light_transition.period, 0, 0, 0);
                g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
            } else if (g_map_ambient_light_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                battle_map_blend_ambient_light_color(MAP_COLOR_MODE_BASE_REPEAT, g_map_ambient_light_transition.period,
                    g_map_ambient_light_transition.repeat_color.r, g_map_ambient_light_transition.repeat_color.g,
                    g_map_ambient_light_transition.repeat_color.b);
                g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_TINTED;
            } else {
                transition->active = 0;
            }
        }
    }

    transition = &g_map_darkness_transition;
    /* Barrier: hides the constant so active and the red channel are accessed through s0. */
    __asm__("" : "=r"(transition) : "0"(transition));
    if (transition->active != 0) {
        s32 red_level;

        red_level = transition->channels.red += g_map_darkness_transition.channels.red_step;
        g_map_darkness_transition.channels.green += g_map_darkness_transition.channels.green_step;
        g_map_darkness_transition.channels.blue += g_map_darkness_transition.channels.blue_step;
        gradient.colors[MAP_BACKGROUND_GRADIENT_TOP].r = red_level / 0x10000;
        gradient.colors[MAP_BACKGROUND_GRADIENT_TOP].g = g_map_darkness_transition.channels.green / 0x10000;
        gradient.colors[MAP_BACKGROUND_GRADIENT_TOP].b = g_map_darkness_transition.channels.blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &gradient);
        if (g_map_darkness_transition.period >= 4) {
            if (++g_map_darkness_transition.tick >= g_map_darkness_transition.period >> 2) {
                g_map_darkness_transition.tick = 0;
                if (++g_map_darkness_transition.phase >= 0x20) {
                    if (g_map_darkness_transition.repeat == MAP_COLOR_REPEAT_TINTED) {
                        battle_map_blend_darkness_color(
                            MAP_COLOR_MODE_RESTORE, g_map_darkness_transition.period, 0, 0, 0);
                        g_map_darkness_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_map_darkness_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                        battle_map_blend_darkness_color(MAP_COLOR_MODE_BASE_REPEAT, g_map_darkness_transition.period,
                            g_map_darkness_transition.repeat_color.r, g_map_darkness_transition.repeat_color.g,
                            g_map_darkness_transition.repeat_color.b);
                        g_map_darkness_transition.repeat = MAP_COLOR_REPEAT_TINTED;
                    } else {
                        transition->active = 0;
                    }
                }
            }
        } else if (++g_map_darkness_transition.phase >= g_map_darkness_transition.period * 8) {
            if (g_map_darkness_transition.repeat == MAP_COLOR_REPEAT_TINTED) {
                battle_map_blend_darkness_color(MAP_COLOR_MODE_RESTORE, g_map_darkness_transition.period, 0, 0, 0);
                g_map_darkness_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
            } else if (g_map_darkness_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                battle_map_blend_darkness_color(MAP_COLOR_MODE_BASE_REPEAT, g_map_darkness_transition.period,
                    g_map_darkness_transition.repeat_color.r, g_map_darkness_transition.repeat_color.g,
                    g_map_darkness_transition.repeat_color.b);
                g_map_darkness_transition.repeat = MAP_COLOR_REPEAT_TINTED;
            } else {
                transition->active = 0;
            }
        }
        if (g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) {
            red = g_map_darkness_transition.channels.red / 0x80000;
            green = g_map_darkness_transition.channels.green / 0x80000;
            blue = g_map_darkness_transition.channels.blue / 0x80000;
            g_map_palette_modified_colors[0][0] = red + (green << 5) + (blue << 10) + 0x8000;

            g_map_palette_state.upload_pending = 1;
        }
    }

    for (i = 0; i < MAP_CLUT_ROW_COUNT; i++) {
        if (g_map_palette_state.clut_rows[i].enabled == 0) {
            for (k = 0; k < MAP_CLUTS_PER_ROW; k++) {
                if (g_map_palette_state.clut_rows[i].animations[k].active == 0) {
                    continue;
                }
                if (g_map_palette_state.clut_rows[i].animations[k].mode >= 4) {
                    if (++g_map_palette_state.clut_rows[i].animations[k].delay_counter
                        >= g_map_palette_state.clut_rows[i].animations[k].mode >> 2) {
                        step = g_map_palette_state.clut_rows[i].animations[k].blend_step;
                        g_map_palette_state.clut_rows[i].animations[k].delay_counter = 0;
                        component = &g_map_palette_state.clut_rows[i].components[k * MAP_CLUT_COLOR_COUNT];
                        for (j = 0; j < MAP_CLUT_COLOR_COUNT; j++) {
                            if (component[j].color.r + component[j].color.g + component[j].color.b != 0) {
                                red = component[j].color.r
                                    + g_map_palette_blend_steps_32[component[j].delta_biased.r][step];
                                green = component[j].color.g
                                    + g_map_palette_blend_steps_32[component[j].delta_biased.g][step];
                                blue = component[j].color.b
                                    + g_map_palette_blend_steps_32[component[j].delta_biased.b][step];
                                if ((red | green | blue) == 0) {
                                    blue = 1;
                                }
                                component[j].color.b = blue;
                                component[j].color.r = red;
                                component[j].color.g = green;
                                g_map_palette_modified_colors[i][k * MAP_CLUT_COLOR_COUNT + j]
                                    = red + (green << 5) + (blue << 10) + (component[j].semi_transparent << 15);
                            }
                        }

                        if (++g_map_palette_state.clut_rows[i].animations[k].blend_step
                            >= MAP_PALETTE_BLEND_STEP_COUNT) {
                            if (g_map_palette_state.clut_rows[i].animations[k].repeat == MAP_COLOR_REPEAT_TINTED) {
                                battle_map_modify_palette(MAP_COLOR_MODE_RESTORE,
                                    g_map_palette_state.clut_rows[i].animations[k].mode, i, k, 0, 0, 0, 0);
                                g_map_palette_state.clut_rows[i].animations[k].repeat = MAP_COLOR_REPEAT_RESTORED;
                            } else if (g_map_palette_state.clut_rows[i].animations[k].repeat
                                == MAP_COLOR_REPEAT_RESTORED) {
                                battle_map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT,
                                    g_map_palette_state.clut_rows[i].animations[k].mode, i, k, 0,
                                    g_map_palette_state.clut_rows[i].animations[k].repeat_color.r,
                                    g_map_palette_state.clut_rows[i].animations[k].repeat_color.g,
                                    g_map_palette_state.clut_rows[i].animations[k].repeat_color.b);
                                g_map_palette_state.clut_rows[i].animations[k].repeat = MAP_COLOR_REPEAT_TINTED;
                            } else {
                                g_map_palette_state.clut_rows[i].animations[k].active = 0;
                            }
                        }
                    }
                } else {
                    step = g_map_palette_state.clut_rows[i].animations[k].blend_step;
                    component = &g_map_palette_state.clut_rows[i].components[k * MAP_CLUT_COLOR_COUNT];
                    for (j = 0; j < MAP_CLUT_COLOR_COUNT; j++) {
                        if (component[j].color.r + component[j].color.g + component[j].color.b != 0) {
                            red = component[j].color.r + g_map_palette_blend_steps_8[component[j].delta_biased.r][step];
                            green
                                = component[j].color.g + g_map_palette_blend_steps_8[component[j].delta_biased.g][step];
                            blue
                                = component[j].color.b + g_map_palette_blend_steps_8[component[j].delta_biased.b][step];
                            if ((red | green | blue) == 0) {
                                blue = 1;
                            }
                            component[j].color.b = blue;
                            component[j].color.r = red;
                            component[j].color.g = green;
                            g_map_palette_modified_colors[i][k * MAP_CLUT_COLOR_COUNT + j]
                                = red + (green << 5) + (blue << 10) + (component[j].semi_transparent << 15);
                        }
                    }

                    if (++g_map_palette_state.clut_rows[i].animations[k].blend_step
                        >= MAP_PALETTE_FAST_BLEND_STEP_COUNT) {
                        if (g_map_palette_state.clut_rows[i].animations[k].repeat == MAP_COLOR_REPEAT_TINTED) {
                            battle_map_modify_palette(MAP_COLOR_MODE_RESTORE,
                                g_map_palette_state.clut_rows[i].animations[k].mode, i, k, 0, 0, 0, 0);
                            g_map_palette_state.clut_rows[i].animations[k].repeat = MAP_COLOR_REPEAT_RESTORED;
                        } else if (g_map_palette_state.clut_rows[i].animations[k].repeat == MAP_COLOR_REPEAT_RESTORED) {
                            battle_map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT,
                                g_map_palette_state.clut_rows[i].animations[k].mode, i, k, 0,
                                g_map_palette_state.clut_rows[i].animations[k].repeat_color.r,
                                g_map_palette_state.clut_rows[i].animations[k].repeat_color.g,
                                g_map_palette_state.clut_rows[i].animations[k].repeat_color.b);
                            g_map_palette_state.clut_rows[i].animations[k].repeat = MAP_COLOR_REPEAT_TINTED;
                        } else {
                            g_map_palette_state.clut_rows[i].animations[k].active = 0;
                        }
                    }
                }
                g_map_palette_state.upload_pending = 1;
            }
        } else if (g_map_palette_state.clut_rows[i].animations[0].active != 0) {
            if (g_map_palette_state.clut_rows[i].animations[0].mode >= 4) {
                if (++g_map_palette_state.clut_rows[i].animations[0].delay_counter
                    >= g_map_palette_state.clut_rows[i].animations[0].mode >> 2) {
                    step = g_map_palette_state.clut_rows[i].animations[0].blend_step;
                    g_map_palette_state.clut_rows[i].animations[0].delay_counter = 0;
                    component = g_map_palette_state.clut_rows[i].components;
                    for (j = 0; j < MAP_CLUT_ROW_COLOR_COUNT; j++) {
                        if (component[j].color.r + component[j].color.g + component[j].color.b != 0) {
                            red = component[j].color.r
                                + g_map_palette_blend_steps_32[component[j].delta_biased.r][step];
                            green = component[j].color.g
                                + g_map_palette_blend_steps_32[component[j].delta_biased.g][step];
                            blue = component[j].color.b
                                + g_map_palette_blend_steps_32[component[j].delta_biased.b][step];
                            if ((red | green | blue) == 0) {
                                blue = 1;
                            }
                            component[j].color.r = red;
                            component[j].color.g = green;
                            component[j].color.b = blue;
                            g_map_palette_modified_colors[i][j]
                                = red + (green << 5) + (blue << 10) + (component[j].semi_transparent << 15);
                        }
                    }

                    if (++g_map_palette_state.clut_rows[i].animations[0].blend_step >= MAP_PALETTE_BLEND_STEP_COUNT) {
                        if (g_map_palette_state.clut_rows[i].animations[0].repeat == MAP_COLOR_REPEAT_TINTED) {
                            battle_map_modify_palette(MAP_COLOR_MODE_RESTORE,
                                g_map_palette_state.clut_rows[i].animations[0].mode, i, 0, 1, 0, 0, 0);
                            g_map_palette_state.clut_rows[i].animations[0].repeat = MAP_COLOR_REPEAT_RESTORED;
                        } else if (g_map_palette_state.clut_rows[i].animations[0].repeat == MAP_COLOR_REPEAT_RESTORED) {
                            battle_map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT,
                                g_map_palette_state.clut_rows[i].animations[0].mode, i, 0, 1,
                                g_map_palette_state.clut_rows[i].animations[0].repeat_color.r,
                                g_map_palette_state.clut_rows[i].animations[0].repeat_color.g,
                                g_map_palette_state.clut_rows[i].animations[0].repeat_color.b);
                            g_map_palette_state.clut_rows[i].animations[0].repeat = MAP_COLOR_REPEAT_TINTED;
                        } else {
                            for (j = MAP_CLUTS_PER_ROW - 1; j >= 0; j--) {
                                g_map_palette_state.clut_rows[i].animations[j].active = 0;
                            }
                        }
                    }
                }
            } else {
                step = g_map_palette_state.clut_rows[i].animations[0].blend_step;
                component = g_map_palette_state.clut_rows[i].components;
                for (j = 0; j < MAP_CLUT_ROW_COLOR_COUNT; j++) {
                    if (component[j].color.r + component[j].color.g + component[j].color.b != 0) {
                        red = component[j].color.r + g_map_palette_blend_steps_8[component[j].delta_biased.r][step];
                        green = component[j].color.g + g_map_palette_blend_steps_8[component[j].delta_biased.g][step];
                        blue = component[j].color.b + g_map_palette_blend_steps_8[component[j].delta_biased.b][step];
                        if ((red | green | blue) == 0) {
                            blue = 1;
                        }
                        component[j].color.r = red;
                        component[j].color.g = green;
                        component[j].color.b = blue;
                        g_map_palette_modified_colors[i][j]
                            = red + (green << 5) + (blue << 10) + (component[j].semi_transparent << 15);
                    }
                }

                if (++g_map_palette_state.clut_rows[i].animations[0].blend_step >= MAP_PALETTE_FAST_BLEND_STEP_COUNT) {
                    if (g_map_palette_state.clut_rows[i].animations[0].repeat == MAP_COLOR_REPEAT_TINTED) {
                        battle_map_modify_palette(MAP_COLOR_MODE_RESTORE,
                            g_map_palette_state.clut_rows[i].animations[0].mode, i, 0, 1, 0, 0, 0);
                        g_map_palette_state.clut_rows[i].animations[0].repeat = MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_map_palette_state.clut_rows[i].animations[0].repeat == MAP_COLOR_REPEAT_RESTORED) {
                        battle_map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT,
                            g_map_palette_state.clut_rows[i].animations[0].mode, i, 0, 1,
                            g_map_palette_state.clut_rows[i].animations[0].repeat_color.r,
                            g_map_palette_state.clut_rows[i].animations[0].repeat_color.g,
                            g_map_palette_state.clut_rows[i].animations[0].repeat_color.b);
                        g_map_palette_state.clut_rows[i].animations[0].repeat = MAP_COLOR_REPEAT_TINTED;
                    } else {
                        for (j = MAP_CLUTS_PER_ROW - 1; j >= 0; j--) {
                            g_map_palette_state.clut_rows[i].animations[j].active = 0;
                        }
                    }
                }
            }
            if (i == 0 && (g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER)) {
                red = g_map_darkness_transition.channels.red / 0x80000;
                green = g_map_darkness_transition.channels.green / 0x80000;
                blue = g_map_darkness_transition.channels.blue / 0x80000;
                g_map_palette_modified_colors[0][0] = red + (green << 5) + (blue << 10) + 0x8000;
            }
            g_map_palette_state.upload_pending = 1;
        }
    }
}

/* Stop the blend of CLUT clut_index of a row, or of all sixteen when stop_all is
 * 1. The colors stay as blended so far. */
void battle_map_stop_palette_animation(s32 clut_row, s32 clut_index, s32 stop_all) {
    s32 index;

    if (stop_all == 1) {
        for (index = MAP_CLUTS_PER_ROW - 1; index >= 0; index--) {
            g_map_palette_state.clut_rows[clut_row].animations[index].active = 0;
        }
    } else {
        g_map_palette_state.clut_rows[clut_row].animations[clut_index].active = 0;
    }
}

/* Load the sixteen colors of one CLUT into a map CLUT row: the upload staging
 * (g_map_palette_modified_colors), the row's packed mirror, and its
 * decoded 5-bit components and alpha bit. */
void battle_map_extract_palette_colors(const palette_t palette, s32 clut_row, s32 clut_index) {
    s32 color_index;
    s32 selected_clut;
    s32 i;
    const u16* palette_ptr;
    u16* upload_palette_base;
    u16* upload_row;
    u16* packed_row;
    map_palette_color_components_t* component_row;
    u16 color;
    map_palette_color_components_t* components;

    selected_clut = clut_index;
    i = 0;
    component_row = g_map_palette_state.clut_rows[clut_row].components;
    upload_palette_base = g_map_palette_modified_colors[0];
    upload_row = upload_palette_base + clut_row * MAP_CLUT_ROW_COLOR_COUNT;
    packed_row = g_map_palette_state.clut_rows[clut_row].packed_colors;
    /* Named color fields reload across the stores; decode each packed 16-bit value once. */
    palette_ptr = (const u16*)palette;
    do {
        /* A CLUT entry is ABBBBBGGGGGRRRRR. */
        color = *palette_ptr++;
        /* Multiplication folds this into the loop and removes four target bytes. */
        color_index = selected_clut << 4;
        color_index += i;
        i++;
        upload_row[color_index] = color;
        components = &component_row[color_index];
        packed_row[color_index] = color;
        components->color.r = color & 0x1f;
        components->color.g = (color & 0x3e0) >> 5;
        components->color.b = (color & 0x7c00) >> 10;
        components->semi_transparent = color >> 15;
    } while (i < MAP_CLUT_COLOR_COUNT);
}

/* Load colors into a map CLUT row and mark the rows for upload: the row's
 * sixteen CLUTs (256 colors) when load_all is 1, otherwise the sixteen colors
 * of CLUT clut_index. The blends of the loaded CLUTs stop. Nothing happens while
 * the row's updates are disabled. */
void battle_map_load_palette_data(const palette_t* palettes, s32 clut_row, s32 clut_index, s32 load_all) {
    s32 clear_index;
    s32 i;

    if (g_map_palette_state.clut_rows[clut_row].updates_disabled != 0) {
        return;
    }

    if (load_all == 1) {
        for (clear_index = MAP_CLUTS_PER_ROW - 1; clear_index >= 0; clear_index--) {
            g_map_palette_state.clut_rows[clut_row].animations[clear_index].active = 0;
        }

        for (i = 0; i < MAP_CLUTS_PER_ROW; i++) {
            battle_map_extract_palette_colors(palettes[i], clut_row, i);
        }
    } else {
        g_map_palette_state.clut_rows[clut_row].animations[clut_index].active = 0;
        battle_map_extract_palette_colors(*palettes, clut_row, clut_index);
    }

    g_map_palette_state.upload_pending = 1;
}

void battle_gfx_modify_vram_palette(s32 mode, const rgba5551_t* source, s32 clut_row, s32 clut_index, s32 preset,
    s16 red, s16 green, s16 blue, s32 final_value) {
    s32 i;
    s32 index;
    u16 color;
    s16 out_red;
    s16 out_green;
    s16 out_blue;
    s32 red_part;
    s32 gray;
    s32 gray_red;
    s32 gray_green;
    s32 gray_blue;
    u32 semi_transparent;
    map_palette_color_components_t* component;

    /* Named component reads change the masks, frame and register allocation. */
    if (preset == 1) {
        i = 0;
        do {
            color = *(const u16*)source;
            if (*(const s16*)source != 0) {
                switch (mode) {
                case MAP_COLOR_MODE_BASE_HALF:
                    red_part = color & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green = green + ((color & 0x3e0) >> 6);
                    out_blue = blue + ((color & 0x7c00) >> 11);
                    break;
                case MAP_COLOR_MODE_BASE_GRAY:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case MAP_COLOR_MODE_BASE_GRAY_HALF:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case MAP_COLOR_MODE_BASE:
                default:
                    red_part = color & 0x1f;
                    out_red = red + red_part;
                    out_green = green + ((color & 0x3e0) >> 5);
                    out_blue = blue + ((color & 0x7c00) >> 10);
                    break;
                }
            } else {
                out_red = color & 0x1f;
                out_green = (color & 0x3e0) >> 5;
                out_blue = (color & 0x7c00) >> 10;
            }
            semi_transparent = color >> 15;
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
            if ((out_red | out_green | out_blue) == 0 && (color & 0x7fff)) {
                out_blue = 1;
            }
            if (final_value != 0) {
                g_map_palette_modified_colors[clut_row][i]
                    = out_red + (out_green << 5) + (out_blue << 10) + (semi_transparent << 15);
                if (g_map_palette_state.clut_rows[clut_row].animations[i / MAP_CLUT_COLOR_COUNT].active == 0) {
                    component = &g_map_palette_state.clut_rows[clut_row].components[i];
                    component->color.r = out_red;
                    component->color.g = out_green;
                    component->color.b = out_blue;
                    component->semi_transparent = semi_transparent;
                }
            }
            g_map_palette_state.clut_rows[clut_row].packed_colors[i]
                = out_red + (out_green << 5) + (out_blue << 10) + (semi_transparent << 15);
            i++;
            source++;
        } while (i < MAP_CLUT_ROW_COLOR_COUNT);
    } else {
        i = 0;
        do {
            color = *(const u16*)source;
            if (*(const s16*)source != 0) {
                switch (mode) {
                case MAP_COLOR_MODE_BASE_HALF:
                    red_part = color & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green = green + ((color & 0x3e0) >> 6);
                    out_blue = blue + ((color & 0x7c00) >> 11);
                    break;
                case MAP_COLOR_MODE_BASE_GRAY:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case MAP_COLOR_MODE_BASE_GRAY_HALF:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case MAP_COLOR_MODE_BASE:
                default:
                    red_part = color & 0x1f;
                    out_red = red + red_part;
                    out_green = green + ((color & 0x3e0) >> 5);
                    out_blue = blue + ((color & 0x7c00) >> 10);
                    break;
                }
            } else {
                out_red = color & 0x1f;
                out_green = (color & 0x3e0) >> 5;
                out_blue = (color & 0x7c00) >> 10;
            }
            semi_transparent = color >> 15;
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
            if ((out_red | out_green | out_blue) == 0 && (color & 0x7fff)) {
                out_blue = 1;
            }
            if (final_value != 0) {
                index = clut_index * MAP_CLUT_COLOR_COUNT + i;
                g_map_palette_modified_colors[clut_row][index]
                    = out_red + (out_green << 5) + (out_blue << 10) + (semi_transparent << 15);
                if (g_map_palette_state.clut_rows[clut_row].animations[clut_index].active == 0) {
                    component = &g_map_palette_state.clut_rows[clut_row].components[index];
                    component->color.r = out_red;
                    component->color.g = out_green;
                    component->color.b = out_blue;
                    component->semi_transparent = semi_transparent;
                }
            }
            g_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * MAP_CLUT_COLOR_COUNT + i]
                = out_red + (out_green << 5) + (out_blue << 10) + (semi_transparent << 15);
            i++;
            source++;
        } while (i < MAP_CLUT_COLOR_COUNT);
    }
    g_map_palette_state.upload_pending = 1;
}

/* Write one CLUT of a row from the colors at `source` with the red, green and
 * blue offsets added: battle_gfx_modify_vram_palette in MAP_COLOR_MODE_RESTORE, a
 * mode in which it neither halves nor grays the colors. */
void battle_map_update_palette_from_other(
    const rgba5551_t* source, s32 clut_row, s32 clut_index, s32 preset, s16 red, s16 green, s16 blue, s32 final_value) {
    battle_gfx_modify_vram_palette(
        MAP_COLOR_MODE_RESTORE, source, clut_row, clut_index, preset, red, green, blue, final_value);
}

/* Load the two map background gradient colors for the current map. */
void battle_map_init_background_gradient(void) {
    map_background_gradient_colors_t* colors;

    colors = battle_map_light_state_command(MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT, colors);
    g_map_background_gradient_transition.colors[0] = colors->colors[MAP_BACKGROUND_GRADIENT_TOP];
    g_map_background_gradient_transition.colors[1] = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
    g_map_background_gradient_transition.channels[0].red = colors->colors[MAP_BACKGROUND_GRADIENT_TOP].r << 16;
    g_map_background_gradient_transition.channels[0].green = colors->colors[MAP_BACKGROUND_GRADIENT_TOP].g << 16;
    g_map_background_gradient_transition.channels[0].blue = colors->colors[MAP_BACKGROUND_GRADIENT_TOP].b << 16;
    g_map_background_gradient_transition.channels[1].red = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r << 16;
    g_map_background_gradient_transition.channels[1].green = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g << 16;
    g_map_background_gradient_transition.channels[1].blue = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b << 16;
}

/* Seed the ambient light fade from the color the map loaded: the base color and
 * the 16.16 transition channels take the current GTE back color.
 *
 * The unset `color` is passed as the command's data argument, which the GET
 * command does not read, and then receives the result. */
void battle_map_init_ambient_light(void) {
    CVECTOR* color;

    color = battle_map_light_state_command(MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR, color);
    g_map_ambient_light_color = *color;
    g_map_ambient_light_transition.channels.red = color->r << 16;
    g_map_ambient_light_transition.channels.green = color->g << 16;
    g_map_ambient_light_transition.channels.blue = color->b << 16;
}

/* Seed the darkness fade from the map's current darkness color: the base color
 * and the 16.16 transition channels.
 *
 * The unset `color` is passed as the command's data argument, which the GET
 * command does not read, and then receives the result. */
void battle_map_init_darkness(void) {
    CVECTOR* color;

    color = battle_map_light_state_command(MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR, color);
    g_map_darkness_color = *color;
    g_map_darkness_transition.channels.red = color->r << 16;
    g_map_darkness_transition.channels.green = color->g << 16;
    g_map_darkness_transition.channels.blue = color->b << 16;
}

/* Upload the pending CLUT rows as one image to the lines below the rows, and
 * on the next call move them into place. */
void battle_map_upload_clut_rows(void) {
    RECT rectangle;
    s32* move_pending;

    move_pending = &g_map_palette_state.move_pending;

    if (*move_pending != 0) {
        rectangle.x = 0;
        rectangle.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_COUNT;
        rectangle.w = MAP_CLUT_ROW_COLOR_COUNT;
        rectangle.h = MAP_CLUT_ROW_COUNT;
        MoveImage(&rectangle, 0, MAP_CLUT_ROWS_Y);
        *move_pending = 0;
    }

    if (g_map_palette_state.upload_pending != 0) {
        rectangle.x = 0;
        rectangle.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_COUNT;
        rectangle.w = MAP_CLUT_ROW_COLOR_COUNT;
        rectangle.h = MAP_CLUT_ROW_COUNT;
        LoadImage(&rectangle, (u32*)g_map_palette_modified_colors);
        g_map_palette_state.upload_pending = 0;
        *move_pending = 1;
    }
}

/* Reset the map palette state at step 0 of the map init sequences: no upload or
 * move pending, every row open to updates with its sixteen CLUT animations
 * stopped, the background gradient and ambient light fades and their repeats off,
 * and the eight color rotation slots free.
 *
 * Each row's `enabled` is set, which makes map_update_animations blend the
 * row's 256 colors as one under animation 0. The sprite, unit, unit shadow and
 * action result rows get 0 and blend each of their sixteen CLUTs on its own. Of
 * the stored deltas only the first color's red delta of each row is cleared. */
void battle_map_init_palette_state(void) {
    s32 clut_row;
    s32 animation_index;

    g_map_palette_state.upload_pending = 0;
    g_map_palette_state.move_pending = 0;

    for (clut_row = 0; clut_row < MAP_CLUT_ROW_COUNT; clut_row++) {
        g_map_palette_state.clut_rows[clut_row].updates_disabled = 0;
        g_map_palette_state.clut_rows[clut_row].enabled = 1;

        for (animation_index = MAP_CLUTS_PER_ROW - 1; animation_index >= 0; animation_index--) {
            g_map_palette_state.clut_rows[clut_row].animations[animation_index].active = 0;
        }

        g_map_palette_state.clut_rows[clut_row].components[0].delta_biased.r = 0;
    }

    g_map_background_gradient_transition.active = 0;
    g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_OFF;
    g_map_ambient_light_transition.active = 0;
    g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_OFF;
    g_map_palette_state.clut_rows[MAP_CLUT_ROW_SPRITES].enabled = 0;
    g_map_palette_state.clut_rows[MAP_CLUT_ROW_UNITS].enabled = 0;
    g_map_palette_state.clut_rows[MAP_CLUT_ROW_UNIT_SHADOWS].enabled = 0;
    g_map_palette_state.clut_rows[MAP_CLUT_ROW_ACTION_RESULTS].enabled = 0;
    battle_map_reset_clut_cycles();
}

void battle_map_freeze(void) {
    battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}

/* Resume the map texture animations paused by map_freeze. */
void battle_map_unfreeze(void) {
    battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}

/* Event ColorField: tint or fade all 256 colors of the map CLUT row with
 * map_modify_palette. `first` is its color mode (map_color_mode_e) and
 * `second` its amount: 0 applies the colors at once, another value fades to
 * them. */
void battle_map_color_field(s32 first, s32 second, s16 red, s16 green, s16 blue) {
    battle_map_modify_palette(first, second, MAP_CLUT_ROW_MAP, 0, 1, red, green, blue);
}

/* Tints one unit's two CLUTs through map_modify_palette; `preset_color` is
 * the MAP_COLOR_MODE_* command and `time` its amount.
 *
 * The unit's CLUT of MAP_CLUT_ROW_UNITS takes the offsets as given. Its CLUT of
 * MAP_CLUT_ROW_UNIT_SHADOWS takes the same offsets while the unit stands on a
 * tile with depth, at or below the tile's surface, and the negated offsets
 * otherwise. An absent unit is skipped. */
void battle_gfx_apply_unit_palette_modulation(
    s32 preset_color, s32 time, s32 runtime_id, s16 red, s16 green, s16 blue) {
    SVECTOR screen_coordinates;
    battle_unit_t* unit;
    map_tile_t* tile;

    unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit == 0) {
        return;
    }

    battle_unit_copy_draw_coords(&screen_coordinates, unit);
    tile = battle_map_get_tile_data_pointer(
        screen_coordinates.vx / MAP_TILE_SIZE, screen_coordinates.vz / MAP_TILE_SIZE, unit->map_z);

    battle_map_modify_palette(preset_color, time, MAP_CLUT_ROW_UNITS, runtime_id, 0, red, green, blue);

    if (tile->depth_half_height.depth != 0
        && unit->world_coords.vy >= -(tile->height + tile->depth_half_height.depth) * 12) {
        battle_map_modify_palette(preset_color, time, MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0, red, green, blue);
    } else {
        battle_map_modify_palette(preset_color, time, MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0, -red, -green, -blue);
    }
}

/* battle_gfx_apply_unit_palette_modulation for one unit, or for all sixteen
 * when runtime_id is 16 or more. */
void battle_gfx_apply_unit_palette_modulation_to_units(
    s32 preset_color, s32 time, s32 runtime_id, s16 red, s16 green, s16 blue) {
    s32 index;

    if (runtime_id >= BATTLE_UNIT_RUNTIME_SLOT_COUNT) {
        for (index = 0; index < BATTLE_UNIT_RUNTIME_SLOT_COUNT; index++) {
            battle_gfx_apply_unit_palette_modulation(preset_color, time, index, red, green, blue);
        }
    } else {
        battle_gfx_apply_unit_palette_modulation(preset_color, time, runtime_id, red, green, blue);
    }
}

void battle_map_modify_background_gradient(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue) {
    battle_map_blend_background_gradient_color(mode, frame_duration, red, green, blue);
}

/* map_blend_ambient_light_color with the color offsets narrowed to s16:
 * the entry event MapDarkness calls. */
void battle_map_start_ambient_light_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue) {
    battle_map_blend_ambient_light_color(mode, frame_duration, red, green, blue);
}

/* map_blend_darkness_color with the color offsets narrowed to s16. The
 * arguments are the mode, the frame duration and the red, green and blue offsets. */
void battle_map_start_darkness_blend(s32 first, s32 second, s16 third, s16 fourth, s16 fifth) {
    battle_map_blend_darkness_color(first, second, third, fourth, fifth);
}

/* map_update_palette_colors for all sixteen CLUTs of the map row. */
void battle_map_update_all_palettes_from_color_data(s32 mode, const clut_t color_data) {
    battle_map_update_palette_colors(mode, MAP_CLUT_ROW_MAP, 0, 1, color_data);
}

/* map_update_palette_colors for one CLUT of the unit row. A clut_index of
 * 16 or more applies the same sixteen colors to every unit CLUT. */
void battle_map_update_palette_colors_by_index(s32 mode, s32 clut_index, const palette_t* color_data) {
    s32 index;

    if (clut_index >= MAP_CLUTS_PER_ROW) {
        for (index = 0; index < MAP_CLUTS_PER_ROW; index++) {
            battle_map_update_palette_colors(mode, MAP_CLUT_ROW_UNITS, index, 0, color_data);
        }
    } else {
        battle_map_update_palette_colors(mode, MAP_CLUT_ROW_UNITS, clut_index, 0, color_data);
    }
}

/* Fade the background gradient to `colors` without changing the base pair, unlike
 * map_transition_and_store_background_gradient. */
void battle_map_set_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors) {
    battle_map_transition_background_gradient(frame_duration, colors);
}

/* Fade the ambient light to `color` without changing the base color, unlike
 * map_transition_and_store_ambient_light_color. */
void battle_map_set_ambient_light_color(s32 frame_duration, const CVECTOR* color) {
    battle_map_transition_ambient_light_color(frame_duration, color);
}

/* Fade the map darkness to `color` without changing the base color, unlike
 * map_transition_and_store_darkness_color. */
void battle_map_set_darkness_color(s32 frame_duration, const CVECTOR* color) {
    battle_map_transition_darkness_color(frame_duration, color);
}

s32 battle_state_update_controller_input(void) {
    u32 pressed;
    u32 previous;
    u32 current;
    u32 released;
    battle_unit_t* unit;

    g_input_current_buttons = PadRead(1);
    if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
        script_filter_controller_input(&g_input_current_buttons);
    }

    pressed = g_main_input_pressed_buttons;
    previous = g_input_previous_buttons;
    current = g_input_current_buttons;
    g_main_input_pressed_copy_0 = pressed;
    g_main_input_pressed_copy_1 = pressed;
    g_main_input_pressed_copy_2 = pressed;
    g_main_input_pressed_copy_3 = pressed;
    g_main_input_pressed_buttons = ~previous & current;
    released = ~current & previous;
    g_main_input_previous_copy_0 = previous;
    g_main_input_previous_copy_1 = previous;
    g_main_input_previous_copy_2 = previous;
    g_main_input_previous_copy_3 = previous;
    g_main_input_released_buttons = released;

    if (previous == current) {
        g_main_input_stable_frames++;
    } else {
        g_main_input_stable_frames = 0;
    }

    g_input_previous_buttons = g_input_current_buttons;
    if ((g_main_input_pressed_buttons & PSX_PAD_TRIANGLE) != 0 && g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        unit = battle_unit_get_source();
        if (g_battle_menu_status_enabled != 0 && g_battle_menu_status_requested == 0
            && (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) == 0) {
            main_sound_play_sfx(MAIN_SFX_STATUS_MENU_REQUEST);
            g_battle_menu_status_requested = 1;
        }
    }

    if ((g_input_previous_buttons & (PSX_PAD_START | PSX_PAD_SELECT | PSX_PAD_L1 | PSX_PAD_R1))
        == (PSX_PAD_START | PSX_PAD_SELECT | PSX_PAD_L1 | PSX_PAD_R1)) {
        main_system_reset_game();
    }
    return 1;
}

/* Read pad 1 for the deployment screen and derive the pressed and released
 * buttons; the reset button combination resets the game. Always returns 1, so the
 * -1 that battle_state_run_deployment tests for does not occur. */
s32 battle_state_update_deployment_controller_input(void) {
    u32 previous;
    u32 current;

    current = PadRead(1);
    previous = g_input_previous_buttons;
    g_input_current_buttons = current;
    g_input_previous_buttons = current;
    g_main_input_pressed_buttons = ~previous & current;
    g_main_input_released_buttons = ~current & previous;

    if ((current & MAIN_INPUT_RESET_BUTTONS) == MAIN_INPUT_RESET_BUTTONS) {
        main_system_reset_game();
    }
    return 1;
}

/* Reset the GPU and set up the two frame buffers for a width x height battle
 * screen with the given projection distance and background color.
 *
 * The second buffer lies at y 0xf0 unless the height is 0x1e0. The draw
 * environments start at x -0x80, so primitive x 0x80 is the left screen edge;
 * dithering is on and the automatic background clear is off. The area of
 * g_battle_deployment_clear_rect is cleared to black and, when font printing is
 * enabled, main_gfx_load_efc_fnt loads the print font again.
 * battle_state_run_deployment calls this with 256 x 240 and projection 0x200. */
void battle_state_init_deployment_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue) {
    RECT clear_rect;
    s32 second_y;

    clear_rect = g_battle_deployment_clear_rect;
    second_y = -(height != 0x1e0) & 0xf0;

    SetDispMask(0);
    ResetGraph(1);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_main_gfx_draw_envs[0], -0x80, 0, width + 0x80, height);
    SetDefDispEnv(&g_main_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_main_gfx_draw_envs[1], -0x80, second_y, width + 0x80, height);
    SetDefDispEnv(&g_main_gfx_display_envs[1], 0, 0, width, height);

    g_main_gfx_draw_envs[0].dtd = g_main_gfx_draw_envs[1].dtd = 1;
    g_main_gfx_draw_envs[0].isbg = g_main_gfx_draw_envs[1].isbg = 0;
    setRGB0(&g_main_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_main_gfx_draw_envs[1], red, green, blue);

    ClearImage(&clear_rect, 0, 0, 0);
    DrawSync(0);
    if (g_main_font_print_enabled != 0) {
        main_gfx_load_efc_fnt();
    }
    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
}

/* End a battle frame: wait for drawing and the vertical blank, flip the display
 * and draw environments and draw the ordering table. Returns VSync's result,
 * which the game loop keeps as the frame's hsync count.
 *
 * At animation speed 1 the wait is one vertical blank. Otherwise it is
 * g_main_system_animation_speed blanks, except in the effect and action-execute
 * states: there it is 2 blanks, or 3 while g_battle_frame_pacing_timer runs (4 while it
 * is 16 or more), with the timer counting down each frame, and g_battle_frame_pacing
 * replaces it when larger. The test for a g_battle_frame_pacing of 1 cannot succeed,
 * since the value it is compared with is at least 2. */
s32 battle_state_sync_frame(u32* ordering_table) {
    s32 wait;
    s32 sync_wait;
    s32 timer;
    s32 sync_result;

    if (g_main_system_animation_speed == 1) {
        DrawSync(0);
        sync_result = VSync(0);
    } else {
        DrawSync(0);
        if (g_battle_game_state == BATTLE_GAME_STATE_EFFECT
            || g_battle_game_state == BATTLE_GAME_STATE_ACTION_EXECUTE) {
            timer = g_battle_frame_pacing_timer;
            if (timer >= 0x10) {
                wait = 4;
                g_battle_frame_pacing_timer = timer - 1;
            } else if (timer != 0) {
                wait = 3;
                g_battle_frame_pacing_timer = timer - 1;
            } else {
                wait = 2;
            }
            sync_wait = g_battle_frame_pacing;
            if (wait < sync_wait) {
                if (sync_wait == 1) {
                    sync_wait = 0;
                }
            } else {
                sync_wait = wait;
            }
            sync_result = VSync(sync_wait);
        } else {
            sync_result = VSync(g_main_system_animation_speed);
        }
    }

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
    DrawOTag(ordering_table);
    FntFlush(-1);
    return sync_result;
}

/* End a frame of the deployment screen: wait for drawing and one vertical blank,
 * flip the display and draw environments and draw the ordering table. Returns
 * VSync's hsync count. */
s32 battle_state_sync_and_submit_deployment_frame(u32* ordering_table) {
    s32 sync_result;

    DrawSync(0);
    sync_result = VSync(0);
    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
    DrawOTag(ordering_table);
    FntFlush(-1);
    return sync_result;
}
