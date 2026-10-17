/* Map palettes, including background gradient, ambient light and darkness transitions, CLUT cycles, VRAM palette
 * uploads, unit palette modulation, and the deployment frame loop. */

#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/battle_map_mesh.h"
#include "fft/battle_map_palette.h"
#include "fft/command.h"
#include "fft/deploy.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/state.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/weather.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* A palette blend moves each five-bit channel by its change (-31..31) over 32 frames, or over 8
 * for the fast blend, adding one table entry per frame. PALETTE_BLEND_POSITION is how far the
 * channel has moved after `step` of `count` frames: the change times the elapsed fraction, rounded
 * down for a rising channel. For a falling channel the original truncates toward zero and then
 * subtracts one, a floor that is one too low at exact multiples, so a change of -8 over 8 frames
 * runs -2, -1, ..., -1, 0 instead of eight -1s. The tables keep that. */
#define PALETTE_BLEND_POSITION(delta, step, count)                                                                     \
    ((step) == 0                   ? 0                                                                                 \
            : (step) == (count)    ? (delta)                                                                           \
            : (delta) * (step) < 0 ? (delta) * (step) / (count) - 1                                                    \
                                   : (delta) * (step) / (count))

/* The change applied on frame `step`. */
#define PALETTE_BLEND_STEP(delta, step, count)                                                                         \
    (PALETTE_BLEND_POSITION(delta, (step) + 1, count) - PALETTE_BLEND_POSITION(delta, step, count))

#define PALETTE_BLEND_STEPS_8(delta, first, count)                                                                     \
    PALETTE_BLEND_STEP(delta, (first) + 0, count), PALETTE_BLEND_STEP(delta, (first) + 1, count),                      \
        PALETTE_BLEND_STEP(delta, (first) + 2, count), PALETTE_BLEND_STEP(delta, (first) + 3, count),                  \
        PALETTE_BLEND_STEP(delta, (first) + 4, count), PALETTE_BLEND_STEP(delta, (first) + 5, count),                  \
        PALETTE_BLEND_STEP(delta, (first) + 6, count), PALETTE_BLEND_STEP(delta, (first) + 7, count)
#define PALETTE_BLEND_ROW_32(delta)                                                                                    \
    { PALETTE_BLEND_STEPS_8(delta, 0, 32), PALETTE_BLEND_STEPS_8(delta, 8, 32), PALETTE_BLEND_STEPS_8(delta, 16, 32),  \
        PALETTE_BLEND_STEPS_8(delta, 24, 32) }
#define PALETTE_BLEND_ROW_8(delta) { PALETTE_BLEND_STEPS_8(delta, 0, 8) }

/* One row per change, -31 first, so a row index is the change plus BATTLE_MAP_PALETTE_DELTA_BIAS. */
#define PALETTE_BLEND_ROWS(ROW)                                                                                        \
    ROW(-31), ROW(-30), ROW(-29), ROW(-28), ROW(-27), ROW(-26), ROW(-25), ROW(-24), ROW(-23), ROW(-22), ROW(-21),      \
        ROW(-20), ROW(-19), ROW(-18), ROW(-17), ROW(-16), ROW(-15), ROW(-14), ROW(-13), ROW(-12), ROW(-11), ROW(-10),  \
        ROW(-9), ROW(-8), ROW(-7), ROW(-6), ROW(-5), ROW(-4), ROW(-3), ROW(-2), ROW(-1), ROW(0), ROW(1), ROW(2),       \
        ROW(3), ROW(4), ROW(5), ROW(6), ROW(7), ROW(8), ROW(9), ROW(10), ROW(11), ROW(12), ROW(13), ROW(14), ROW(15),  \
        ROW(16), ROW(17), ROW(18), ROW(19), ROW(20), ROW(21), ROW(22), ROW(23), ROW(24), ROW(25), ROW(26), ROW(27),    \
        ROW(28), ROW(29), ROW(30), ROW(31)

/* Per-frame channel steps for the 32-frame and the 8-frame blends. */
static s8 g_battle_map_palette_blend_steps_32[BATTLE_MAP_PALETTE_DELTA_COUNT][BATTLE_MAP_PALETTE_BLEND_STEP_COUNT]
    = { PALETTE_BLEND_ROWS(PALETTE_BLEND_ROW_32) };
static s8 g_battle_map_palette_blend_steps_8[BATTLE_MAP_PALETTE_DELTA_COUNT][BATTLE_MAP_PALETTE_FAST_BLEND_STEP_COUNT]
    = { PALETTE_BLEND_ROWS(PALETTE_BLEND_ROW_8) };

/* Decode one CLUT's sixteen colors for a map CLUT row. Mode 0 sets them as the components and
 * upload staging at once and stops the CLUT's blend; another mode stores biased deltas toward them
 * and starts the blend with that mode. */
static void battle_map_palette_update_colors_inner(s32 mode, s32 clut_row, s32 clut_index, const palette_t colors) {
    s32 index = 0;
    s16 green_for_packing;
    u32 blue_delta;
    u16 red_for_packing;

    do {
        s32 color_index;
        u16 color;
        battle_map_palette_color_components_t* component;
        u8 green;
        u32 green_value;
        u32 blue_value;
        u8 red;
        u32 red_value;
        u8 blue;

        color_index = clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + index;
        component = &g_battle_map_palette_state.clut_rows[clut_row].components[color_index];
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
            component->delta_biased.r = red - component->color.r + BATTLE_MAP_PALETTE_DELTA_BIAS;
            blue_delta = blue - component->color.b + BATTLE_MAP_PALETTE_DELTA_BIAS;
            component->delta_biased.g = green - component->color.g + BATTLE_MAP_PALETTE_DELTA_BIAS;
            component->delta_biased.b = blue_delta;
        } else {
            component->color.g = green;
            component->color.r = red;
            component->color.b = blue;
            g_battle_map_palette_modified_colors[clut_row][color_index]
                = red_for_packing + (green_for_packing << 5) + (blue_value << 10) + (component->semi_transparent << 15);
        }

        index++;
        colors++;
    } while (index < BATTLE_MAP_CLUT_COLOR_COUNT);

    if (mode != 0) {
        battle_map_palette_animation_state_t* animation;

        animation = &g_battle_map_palette_state.clut_rows[clut_row].animations[clut_index];
        animation->active = 1;
        animation->blend_step = 0;
        animation->delay_counter = 0;
        animation->mode = mode;
    } else {
        g_battle_map_palette_state.clut_rows[clut_row].animations[clut_index].active = 0;
    }
}

/* Set or blend CLUTs of a map row to `colors`: all sixteen (256 colors) when
 * update_all is 1, otherwise CLUT clut_index (16 colors). Mode 0 applies the
 * colors at once; another value starts a blend toward them and is kept as the
 * animation's mode (battle_map_palette_update_colors_inner). Marks the rows for
 * upload. */
void battle_map_palette_update_colors(s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors) {
    s32 index;

    if (update_all == 1) {
        const palette_t* current_colors;

        index = 0;
        current_colors = colors;
        while (index < BATTLE_MAP_CLUTS_PER_ROW) {
            battle_map_palette_update_colors_inner(mode, clut_row, index, *current_colors);
            index++;
            current_colors++;
        }
        g_battle_map_palette_state.upload_pending = 1;
    } else {
        battle_map_palette_update_colors_inner(mode, clut_row, clut_index, *colors);
        g_battle_map_palette_state.upload_pending = 1;
    }
}

/* battle_map_palette_update_colors, then copy the colors into the row's packed
 * mirror as well, which makes them the base colors that the BATTLE_MAP_COLOR_MODE_BASE
 * modes and RESTORE of battle_map_palette_modify start from: all 256 when
 * update_all is 1, otherwise the 16 of CLUT clut_index. */
void battle_map_palette_update_and_store_colors(
    s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors) {
    s32 index;
    const palette_t* colors_reg = colors;

    battle_map_palette_update_colors(mode, clut_row, clut_index, update_all, colors_reg);

    /* The packed mirror is copied with native 16-bit loads and stores. */
    if (update_all == 1) {
        u16* destination;
        const u16* source;

        index = 0;
        destination = g_battle_map_palette_state.clut_rows[clut_row].packed_colors;
        source = (const u16*)colors_reg;
        while (index < BATTLE_MAP_CLUT_ROW_COLOR_COUNT) {
            *destination++ = *source++;
            index++;
        }
    } else {
        const u16* source;
        u16* palette_colors;
        s32 palette_offset;

        index = 0;
        palette_offset = clut_index * BATTLE_MAP_CLUT_COLOR_COUNT;
        palette_colors = g_battle_map_palette_state.clut_rows[clut_row].packed_colors;
        source = (const u16*)colors_reg;
        while (index < BATTLE_MAP_CLUT_COLOR_COUNT) {
            palette_colors[palette_offset + index] = *source++;
            index++;
        }
    }
}

/* Make the colors on display (the upload staging) the base colors of a CLUT row, which the
 * BATTLE_MAP_COLOR_MODE_BASE modes and RESTORE start from: all 256 when store_all is 1, otherwise CLUT
 * clut_index. Event instruction 0x66 stores the whole map row. */
void battle_map_palette_store_modified_colors(s32 clut_row, s32 clut_index, s32 store_all) {
    if (store_all == 1) {
        u16* destination;
        u16* source;

        store_all = 0;
        destination = g_battle_map_palette_state.clut_rows[clut_row].packed_colors;
        source = g_battle_map_palette_modified_colors[0];
        source += clut_row * BATTLE_MAP_CLUT_ROW_COLOR_COUNT;
        while (store_all < BATTLE_MAP_CLUT_ROW_COLOR_COUNT) {
            *destination++ = *source++;
            store_all++;
        }
    } else {
        u16* packed_row;

        store_all = 0;
        clut_index *= BATTLE_MAP_CLUT_COLOR_COUNT;
        packed_row = g_battle_map_palette_state.clut_rows[clut_row].packed_colors;
        while (store_all < BATTLE_MAP_CLUT_COLOR_COUNT) {
            packed_row[clut_index + store_all] = g_battle_map_palette_modified_colors[clut_row][clut_index + store_all];
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
 * become neutral), and a result that would be black takes blue 1, since the
 * color value 0 is transparent. A nonzero amount stores biased deltas and arms the CLUT
 * animations with that step; zero applies the colors immediately and marks
 * the palette upload pending.
 *
 * The packed-color reads repeat `clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i` (no index local) and
 * the gray cases use case-local temporaries: both keep the target's loop
 * strength reduction and local register allocation. */
void battle_map_palette_modify(
    s32 command, s32 amount, s32 clut_row, s32 clut_index, s32 all_colors, s16 red, s16 green, s16 blue) {
    s32 i;
    s16 out_red;
    s16 out_green;
    s16 out_blue;
    s32 red_part;
    battle_map_palette_color_components_t* components;
    battle_map_palette_color_components_t* clut;
    battle_map_palette_color_components_t* component;
    battle_map_palette_animation_state_t* animation;

    if (all_colors == 1) {
        components = g_battle_map_palette_state.clut_rows[clut_row].components;
        for (i = 0; i < BATTLE_MAP_CLUT_ROW_COLOR_COUNT; i++) {
            if (components[i].color.r + components[i].color.g + components[i].color.b != 0) {
                switch (command) {
                case BATTLE_MAP_COLOR_MODE_CURRENT:
                    out_red = components[i].color.r + red;
                    out_green = components[i].color.g + green;
                    out_blue = components[i].color.b + blue;
                    break;
                case BATTLE_MAP_COLOR_MODE_CURRENT_HALF:
                    out_red = (components[i].color.r >> 1) + red;
                    out_green = (components[i].color.g >> 1) + green;
                    out_blue = (components[i].color.b >> 1) + blue;
                    break;
                case BATTLE_MAP_COLOR_MODE_CURRENT_GRAY: {
                    s32 gray;
                    gray = (components[i].color.r * 2 + components[i].color.g * 3 + components[i].color.b) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case BATTLE_MAP_COLOR_MODE_CURRENT_GRAY_HALF: {
                    s32 gray;
                    gray = (components[i].color.r * 2 + components[i].color.g * 3 + components[i].color.b) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case BATTLE_MAP_COLOR_MODE_BASE:
                case BATTLE_MAP_COLOR_MODE_BASE_REPEAT:
                    out_red = red + (g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x1f);
                    out_green
                        = green + ((g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x3e0) >> 5);
                    out_blue
                        = blue + ((g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x7c00) >> 10);
                    break;
                case BATTLE_MAP_COLOR_MODE_BASE_HALF:
                    red_part = g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green
                        = green + ((g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x3e0) >> 6);
                    out_blue
                        = blue + ((g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x7c00) >> 11);
                    break;
                case BATTLE_MAP_COLOR_MODE_BASE_GRAY: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits = g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x3e0;
                    blue_bits = g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x7c00;
                    red_bits = g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x1f;
                    gray = red_bits * 2 + (green_bits >> 5) * 3 + (blue_bits >> 10);
                    gray /= 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case BATTLE_MAP_COLOR_MODE_BASE_GRAY_HALF: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits = g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x3e0;
                    blue_bits = g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x7c00;
                    red_bits = g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x1f;
                    gray = red_bits * 2 + (green_bits >> 5) * 3 + (blue_bits >> 10);
                    gray /= 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case BATTLE_MAP_COLOR_MODE_RESTORE:
                    out_red = g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x1f;
                    out_green = (g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x3e0) >> 5;
                    out_blue = (g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i] & 0x7c00) >> 10;
                    break;
                case BATTLE_MAP_COLOR_MODE_STOP_REPEAT:
                    for (i = BATTLE_MAP_CLUTS_PER_ROW - 1; i >= 0; i--) {
                        g_battle_map_palette_state.clut_rows[clut_row].animations[i].repeat
                            = BATTLE_MAP_COLOR_REPEAT_OFF;
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
                    components[i].delta_biased.r = out_red - components[i].color.r + BATTLE_MAP_PALETTE_DELTA_BIAS;
                    components[i].delta_biased.g = out_green - components[i].color.g + BATTLE_MAP_PALETTE_DELTA_BIAS;
                    components[i].delta_biased.b = out_blue - components[i].color.b + BATTLE_MAP_PALETTE_DELTA_BIAS;
                } else {
                    components[i].color.r = out_red;
                    components[i].color.g = out_green;
                    components[i].color.b = out_blue;
                    g_battle_map_palette_modified_colors[clut_row][i]
                        = out_red + (out_green << 5) + (out_blue << 10) + (components[i].semi_transparent << 15);
                }
            } else {
                components[i].delta_biased.r = components[i].delta_biased.g = components[i].delta_biased.b
                    = BATTLE_MAP_PALETTE_DELTA_BIAS;
            }
        }
        if (amount != 0) {
            for (i = 0; i < BATTLE_MAP_CLUTS_PER_ROW; i++) {
                animation = &g_battle_map_palette_state.clut_rows[clut_row].animations[i];
                animation->active = 1;
                animation->blend_step = 0;
                animation->delay_counter = 0;
                animation->mode = amount;
                if (command == BATTLE_MAP_COLOR_MODE_BASE_REPEAT) {
                    animation->repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
                    animation->repeat_color.r = red;
                    animation->repeat_color.g = green;
                    animation->repeat_color.b = blue;
                }
            }
            return;
        }
        for (i = BATTLE_MAP_CLUTS_PER_ROW - 1; i >= 0; i--) {
            g_battle_map_palette_state.clut_rows[clut_row].animations[i].active = 0;
        }
        g_battle_map_palette_state.upload_pending = 1;
    } else {
        clut = &g_battle_map_palette_state.clut_rows[clut_row].components[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT];
        for (i = 0; i < BATTLE_MAP_CLUT_COLOR_COUNT; i++) {
            component = &clut[i];
            if (component->color.r + component->color.g + component->color.b != 0) {
                switch (command) {
                case BATTLE_MAP_COLOR_MODE_CURRENT:
                    out_red = component->color.r + red;
                    out_green = component->color.g + green;
                    out_blue = component->color.b + blue;
                    break;
                case BATTLE_MAP_COLOR_MODE_CURRENT_HALF:
                    out_red = (component->color.r >> 1) + red;
                    out_green = (component->color.g >> 1) + green;
                    out_blue = (component->color.b >> 1) + blue;
                    break;
                case BATTLE_MAP_COLOR_MODE_CURRENT_GRAY: {
                    s32 gray;
                    gray = (component->color.r * 2 + component->color.g * 3 + component->color.b) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case BATTLE_MAP_COLOR_MODE_CURRENT_GRAY_HALF: {
                    s32 gray;
                    gray = (component->color.r * 2 + component->color.g * 3 + component->color.b) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case BATTLE_MAP_COLOR_MODE_BASE:
                case BATTLE_MAP_COLOR_MODE_BASE_REPEAT:
                    out_red = red
                        + (g_battle_map_palette_state.clut_rows[clut_row]
                                .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                            & 0x1f);
                    out_green = green
                        + ((g_battle_map_palette_state.clut_rows[clut_row]
                                   .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                               & 0x3e0)
                            >> 5);
                    out_blue = blue
                        + ((g_battle_map_palette_state.clut_rows[clut_row]
                                   .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                               & 0x7c00)
                            >> 10);
                    break;
                case BATTLE_MAP_COLOR_MODE_BASE_HALF:
                    red_part = g_battle_map_palette_state.clut_rows[clut_row]
                                   .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                        & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green = green
                        + ((g_battle_map_palette_state.clut_rows[clut_row]
                                   .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                               & 0x3e0)
                            >> 6);
                    out_blue = blue
                        + ((g_battle_map_palette_state.clut_rows[clut_row]
                                   .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                               & 0x7c00)
                            >> 11);
                    break;
                case BATTLE_MAP_COLOR_MODE_BASE_GRAY: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits = g_battle_map_palette_state.clut_rows[clut_row]
                                     .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                        & 0x3e0;
                    blue_bits = g_battle_map_palette_state.clut_rows[clut_row]
                                    .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                        & 0x7c00;
                    red_bits = g_battle_map_palette_state.clut_rows[clut_row]
                                   .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                        & 0x1f;
                    gray = red_bits * 2 + (green_bits >> 5) * 3 + (blue_bits >> 10);
                    gray /= 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case BATTLE_MAP_COLOR_MODE_BASE_GRAY_HALF: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits = g_battle_map_palette_state.clut_rows[clut_row]
                                     .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                        & 0x3e0;
                    blue_bits = g_battle_map_palette_state.clut_rows[clut_row]
                                    .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                        & 0x7c00;
                    red_bits = g_battle_map_palette_state.clut_rows[clut_row]
                                   .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                        & 0x1f;
                    gray = red_bits * 2 + (green_bits >> 5) * 3 + (blue_bits >> 10);
                    gray /= 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case BATTLE_MAP_COLOR_MODE_RESTORE:
                    out_red = g_battle_map_palette_state.clut_rows[clut_row]
                                  .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                        & 0x1f;
                    out_green = (g_battle_map_palette_state.clut_rows[clut_row]
                                        .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                                    & 0x3e0)
                        >> 5;
                    out_blue = (g_battle_map_palette_state.clut_rows[clut_row]
                                       .packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                                   & 0x7c00)
                        >> 10;
                    break;
                case BATTLE_MAP_COLOR_MODE_STOP_REPEAT:
                    g_battle_map_palette_state.clut_rows[clut_row].animations[clut_index].repeat
                        = BATTLE_MAP_COLOR_REPEAT_OFF;
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
                    component->delta_biased.r = out_red - component->color.r + BATTLE_MAP_PALETTE_DELTA_BIAS;
                    component->delta_biased.g = out_green - component->color.g + BATTLE_MAP_PALETTE_DELTA_BIAS;
                    component->delta_biased.b = out_blue - component->color.b + BATTLE_MAP_PALETTE_DELTA_BIAS;
                } else {
                    component->color.r = out_red;
                    component->color.g = out_green;
                    component->color.b = out_blue;
                    g_battle_map_palette_modified_colors[clut_row][clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                        = out_red + (out_green << 5) + (out_blue << 10) + (component->semi_transparent << 15);
                }
            } else {
                component->delta_biased.r = component->delta_biased.g = component->delta_biased.b
                    = BATTLE_MAP_PALETTE_DELTA_BIAS;
            }
        }
        if (amount != 0) {
            animation = &g_battle_map_palette_state.clut_rows[clut_row].animations[clut_index];
            animation->active = 1;
            animation->blend_step = 0;
            animation->delay_counter = 0;
            animation->mode = amount;
            if (command == BATTLE_MAP_COLOR_MODE_BASE_REPEAT) {
                animation->repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
                animation->repeat_color.r = red;
                animation->repeat_color.g = green;
                animation->repeat_color.b = blue;
            }
            return;
        }
        g_battle_map_palette_state.clut_rows[clut_row].animations[clut_index].active = 0;
        g_battle_map_palette_state.upload_pending = 1;
    }
}

/*
 * Starts a fade of both background gradient colors toward `colors` over
 * frame_duration * 8 frames, or applies them at once when frame_duration is
 * zero. Indexing base[i]/color[i] (not advancing pointers) is required: a
 * pointer walk splits into biased givs.
 */
static void battle_map_background_transition(
    s32 frame_duration, const battle_map_background_gradient_colors_t* colors) {
    battle_map_color_transition_channels_t* base;
    const CVECTOR* color;
    battle_map_background_gradient_colors_t current;
    s32 i;
    s32 red;
    s32 green;
    s32 blue;
    s32 target_red;
    s32 target_green;
    s32 target_blue;

    base = g_battle_map_background_transition.channels;
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
        g_battle_map_background_transition.active = 1;
        g_battle_map_background_transition.phase = 0;
        g_battle_map_background_transition.tick = 0;
        g_battle_map_background_transition.period = frame_duration;
    } else {
        g_battle_map_background_transition.active = 0;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r = base[0].red / 0x10000;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g = base[0].green / 0x10000;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b = base[0].blue / 0x10000;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].r = base[1].red / 0x10000;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].g = base[1].green / 0x10000;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].b = base[1].blue / 0x10000;
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &current);
    }
}

/* Fade the background gradient to `colors` and make them the base pair that the
 * BATTLE_MAP_COLOR_MODE_BASE modes and RESTORE start from. */
void battle_map_background_transition_and_store(
    s32 frame_duration, const battle_map_background_gradient_colors_t* colors) {
    battle_map_background_transition(frame_duration, colors);
    g_battle_map_background_transition.colors[0] = colors->colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP];
    g_battle_map_background_transition.colors[1] = colors->colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM];
}

/* Fade both background gradient colors toward a target that mode (battle_map_color_mode_e) builds from
 * the current or base colors plus the offsets, clamped to 0..255, over frame_duration * 8 frames,
 * or set it at once for 0. BASE_REPEAT records the offsets so the fade keeps alternating with the
 * base. */
static void battle_map_background_blend_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue) {
    battle_map_color_transition_channels_t* channels;
    battle_map_background_gradient_colors_t current;
    s32 target_red;
    s32 target_green;
    s32 target_blue;
    s32 base_red;
    s32 base_green;
    s32 base_blue;
    s32 i;

    channels = g_battle_map_background_transition.channels;
    for (i = 0; i < 2; i++) {
        switch (mode) {
        case BATTLE_MAP_COLOR_MODE_CURRENT:
            target_red = (red << 16) + channels[i].red;
            target_green = (green << 16) + channels[i].green;
            target_blue = (blue << 16) + channels[i].blue;
            break;
        case BATTLE_MAP_COLOR_MODE_CURRENT_HALF:
            target_red = channels[i].red / 2 + (red << 16);
            target_green = channels[i].green / 2 + (green << 16);
            target_blue = channels[i].blue / 2 + (blue << 16);
            break;
        case BATTLE_MAP_COLOR_MODE_CURRENT_GRAY: {
            s32 gray = (channels[i].red * 2 + channels[i].green * 3 + channels[i].blue) / 6;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case BATTLE_MAP_COLOR_MODE_CURRENT_GRAY_HALF: {
            s32 gray = (channels[i].red * 2 + channels[i].green * 3 + channels[i].blue) / 12;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case BATTLE_MAP_COLOR_MODE_BASE:
        case BATTLE_MAP_COLOR_MODE_BASE_REPEAT:
            target_red = (g_battle_map_background_transition.colors[i].r + red) << 16;
            target_green = (g_battle_map_background_transition.colors[i].g + green) << 16;
            target_blue = (g_battle_map_background_transition.colors[i].b + blue) << 16;
            break;
        case BATTLE_MAP_COLOR_MODE_BASE_HALF:
            target_red = (g_battle_map_background_transition.colors[i].r << 15) + (red << 16);
            target_green = (g_battle_map_background_transition.colors[i].g << 15) + (green << 16);
            target_blue = (g_battle_map_background_transition.colors[i].b << 15) + (blue << 16);
            break;
        case BATTLE_MAP_COLOR_MODE_BASE_GRAY: {
            s32 gray;

            base_red = g_battle_map_background_transition.colors[i].r << 16;
            base_green = g_battle_map_background_transition.colors[i].g << 16;
            base_blue = g_battle_map_background_transition.colors[i].b << 16;
            gray = (base_red * 2 + base_green * 3 + base_blue) / 6;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case BATTLE_MAP_COLOR_MODE_BASE_GRAY_HALF: {
            s32 gray;

            base_red = g_battle_map_background_transition.colors[i].r << 16;
            base_green = g_battle_map_background_transition.colors[i].g << 16;
            base_blue = g_battle_map_background_transition.colors[i].b << 16;
            gray = (base_red * 2 + base_green * 3 + base_blue) / 12;
            target_red = gray + (red << 16);
            target_green = gray + (green << 16);
            target_blue = gray + (blue << 16);
            break;
        }
        case BATTLE_MAP_COLOR_MODE_RESTORE:
            target_red = g_battle_map_background_transition.colors[i].r << 16;
            target_green = g_battle_map_background_transition.colors[i].g << 16;
            target_blue = g_battle_map_background_transition.colors[i].b << 16;
            break;
        case BATTLE_MAP_COLOR_MODE_STOP_REPEAT:
            g_battle_map_background_transition.repeat = BATTLE_MAP_COLOR_REPEAT_OFF;
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
        g_battle_map_background_transition.active = 1;
        g_battle_map_background_transition.phase = 0;
        g_battle_map_background_transition.tick = 0;
        g_battle_map_background_transition.period = frame_duration;
        if (mode == BATTLE_MAP_COLOR_MODE_BASE_REPEAT) {
            g_battle_map_background_transition.repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
            g_battle_map_background_transition.repeat_color.r = red;
            g_battle_map_background_transition.repeat_color.g = green;
            g_battle_map_background_transition.repeat_color.b = blue;
        }
    } else {
        g_battle_map_background_transition.active = 0;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r = channels[0].red / 0x10000;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g = channels[0].green / 0x10000;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b = channels[0].blue / 0x10000;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].r = channels[1].red / 0x10000;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].g = channels[1].green / 0x10000;
        current.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].b = channels[1].blue / 0x10000;
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &current);
    }
}

/* Start a fade of the ambient light toward color over frame_duration * 8
 * frames, or apply it at once when frame_duration is zero. */
static void battle_map_ambient_transition(s32 frame_duration, const CVECTOR* color) {
    battle_map_color_transition_channels_t* channels;
    CVECTOR current;
    s32 red;
    s32 green;
    s32 blue;
    s32 target_red;
    s32 target_green;
    s32 target_blue;

    channels = &g_battle_map_ambient_transition.channels;
    /* The separate target copies reproduce the target's register copies that
     * feed the immediate-store path. */
    red = color->r << 16;
    target_red = red;
    green = color->g << 16;
    target_green = green;
    blue = color->b << 16;
    target_blue = blue;
    if (frame_duration != 0) {
        g_battle_map_ambient_transition.channels.red_step = (red - (channels->red & ~0xFFFF)) / (frame_duration * 8);
        g_battle_map_ambient_transition.channels.green_step
            = (green - (channels->green & ~0xFFFF)) / (frame_duration * 8);
        g_battle_map_ambient_transition.channels.blue_step = (blue - (channels->blue & ~0xFFFF)) / (frame_duration * 8);
    } else {
        channels->red = target_red;
        channels->green = target_green;
        channels->blue = target_blue;
    }
    if (frame_duration != 0) {
        g_battle_map_ambient_transition.active = 1;
        g_battle_map_ambient_transition.phase = 0;
        g_battle_map_ambient_transition.tick = 0;
        g_battle_map_ambient_transition.period = frame_duration;
    } else {
        g_battle_map_ambient_transition.active = 0;
        current.r = channels->red / 0x10000;
        current.g = channels->green / 0x10000;
        current.b = channels->blue / 0x10000;
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, &current);
    }
}

/* Fade the ambient light to `color` and make it the base color that the
 * BATTLE_MAP_COLOR_MODE_BASE modes and RESTORE start from. */
void battle_map_ambient_transition_and_store(s32 frame_duration, const CVECTOR* color) {
    battle_map_ambient_transition(frame_duration, color);
    g_battle_map_ambient_base_color = *color;
}

/* As battle_map_background_blend_color, for the ambient light color. Event MapDarkness
 * reaches it through battle_map_ambient_start_blend. */
static void battle_map_ambient_blend_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue) {
    battle_map_color_transition_channels_t* channels;
    CVECTOR current;
    s32 target_red;
    s32 target_green;
    s32 target_blue;
    s32 gray;
    s32 base_red;
    s32 base_green;
    s32 base_blue;

    channels = &g_battle_map_ambient_transition.channels;
    switch (mode) {
    case BATTLE_MAP_COLOR_MODE_CURRENT:
        target_red = (red << 16) + channels->red;
        target_green = (green << 16) + channels->green;
        target_blue = (blue << 16) + channels->blue;
        break;
    case BATTLE_MAP_COLOR_MODE_CURRENT_HALF:
        target_red = channels->red / 2 + (red << 16);
        target_green = channels->green / 2 + (green << 16);
        target_blue = channels->blue / 2 + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_CURRENT_GRAY:
        gray = (channels->red * 2 + channels->green * 3 + channels->blue) / 6;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_CURRENT_GRAY_HALF:
        gray = (channels->red * 2 + channels->green * 3 + channels->blue) / 12;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_BASE:
    case BATTLE_MAP_COLOR_MODE_BASE_REPEAT:
        target_red = (g_battle_map_ambient_base_color.r + red) << 16;
        target_green = (g_battle_map_ambient_base_color.g + green) << 16;
        target_blue = (g_battle_map_ambient_base_color.b + blue) << 16;
        break;
    case BATTLE_MAP_COLOR_MODE_BASE_HALF:
        /* The base color is unsigned, so the halved 16.16 value is a shift. */
        target_red = (g_battle_map_ambient_base_color.r << 15) + (red << 16);
        target_green = (g_battle_map_ambient_base_color.g << 15) + (green << 16);
        target_blue = (g_battle_map_ambient_base_color.b << 15) + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_BASE_GRAY:
        base_red = g_battle_map_ambient_base_color.r << 16;
        base_green = g_battle_map_ambient_base_color.g << 16;
        base_blue = g_battle_map_ambient_base_color.b << 16;
        gray = (base_red * 2 + base_green * 3 + base_blue) / 6;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_BASE_GRAY_HALF:
        base_red = g_battle_map_ambient_base_color.r << 16;
        base_green = g_battle_map_ambient_base_color.g << 16;
        base_blue = g_battle_map_ambient_base_color.b << 16;
        gray = (base_red * 2 + base_green * 3 + base_blue) / 12;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_RESTORE:
        target_red = g_battle_map_ambient_base_color.r << 16;
        target_green = g_battle_map_ambient_base_color.g << 16;
        target_blue = g_battle_map_ambient_base_color.b << 16;
        break;
    case BATTLE_MAP_COLOR_MODE_STOP_REPEAT:
        g_battle_map_ambient_transition.repeat = BATTLE_MAP_COLOR_REPEAT_OFF;
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
        g_battle_map_ambient_transition.active = 1;
        g_battle_map_ambient_transition.phase = 0;
        g_battle_map_ambient_transition.tick = 0;
        g_battle_map_ambient_transition.period = frame_duration;
        if (mode == BATTLE_MAP_COLOR_MODE_BASE_REPEAT) {
            g_battle_map_ambient_transition.repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
            g_battle_map_ambient_transition.repeat_color.r = red;
            g_battle_map_ambient_transition.repeat_color.g = green;
            g_battle_map_ambient_transition.repeat_color.b = blue;
        }
    } else {
        g_battle_map_ambient_transition.active = 0;
        current.r = channels->red / 0x10000;
        current.g = channels->green / 0x10000;
        current.b = channels->blue / 0x10000;
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, &current);
    }
}

/* Start a fade of the map darkness toward color over frame_duration * 8
 * frames, or apply it at once when frame_duration is zero. */
static void battle_map_darkness_transition(s32 frame_duration, const CVECTOR* color) {
    battle_map_color_transition_channels_t* channels;
    CVECTOR current;
    s32 red;
    s32 green;
    s32 blue;
    s32 target_red;
    s32 target_green;
    s32 target_blue;

    channels = &g_battle_map_darkness_transition.channels;
    /* The separate target copies reproduce the target's register copies that
     * feed the immediate-store path. */
    red = color->r << 16;
    target_red = red;
    green = color->g << 16;
    target_green = green;
    blue = color->b << 16;
    target_blue = blue;
    if (frame_duration != 0) {
        g_battle_map_darkness_transition.channels.red_step = (red - (channels->red & ~0xFFFF)) / (frame_duration * 8);
        g_battle_map_darkness_transition.channels.green_step
            = (green - (channels->green & ~0xFFFF)) / (frame_duration * 8);
        g_battle_map_darkness_transition.channels.blue_step
            = (blue - (channels->blue & ~0xFFFF)) / (frame_duration * 8);
    } else {
        channels->red = target_red;
        channels->green = target_green;
        channels->blue = target_blue;
    }
    if (frame_duration != 0) {
        g_battle_map_darkness_transition.active = 1;
        g_battle_map_darkness_transition.phase = 0;
        g_battle_map_darkness_transition.tick = 0;
        g_battle_map_darkness_transition.period = frame_duration;
    } else {
        g_battle_map_darkness_transition.active = 0;
        current.r = channels->red / 0x10000;
        current.g = channels->green / 0x10000;
        current.b = channels->blue / 0x10000;
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &current);
    }
}

/* Fade the map darkness to `color` and make it the base color that the
 * BATTLE_MAP_COLOR_MODE_BASE modes and RESTORE start from. */
void battle_map_darkness_transition_and_store(s32 frame_duration, const CVECTOR* color) {
    battle_map_darkness_transition(frame_duration, color);
    g_battle_map_darkness_base_color = *color;
}

/* As battle_map_background_blend_color, for the map darkness color
 * (battle_map_darkness_start_blend). */
static void battle_map_darkness_blend_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue) {
    battle_map_color_transition_channels_t* channels;
    CVECTOR current;
    s32 target_red;
    s32 target_green;
    s32 target_blue;
    s32 gray;
    s32 base_red;
    s32 base_green;
    s32 base_blue;

    channels = &g_battle_map_darkness_transition.channels;
    switch (mode) {
    case BATTLE_MAP_COLOR_MODE_CURRENT:
        target_red = (red << 16) + channels->red;
        target_green = (green << 16) + channels->green;
        target_blue = (blue << 16) + channels->blue;
        break;
    case BATTLE_MAP_COLOR_MODE_CURRENT_HALF:
        target_red = channels->red / 2 + (red << 16);
        target_green = channels->green / 2 + (green << 16);
        target_blue = channels->blue / 2 + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_CURRENT_GRAY:
        gray = (channels->red * 2 + channels->green * 3 + channels->blue) / 6;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_CURRENT_GRAY_HALF:
        gray = (channels->red * 2 + channels->green * 3 + channels->blue) / 12;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_BASE:
    case BATTLE_MAP_COLOR_MODE_BASE_REPEAT:
        target_red = (g_battle_map_darkness_base_color.r + red) << 16;
        target_green = (g_battle_map_darkness_base_color.g + green) << 16;
        target_blue = (g_battle_map_darkness_base_color.b + blue) << 16;
        break;
    case BATTLE_MAP_COLOR_MODE_BASE_HALF:
        /* The base color is unsigned, so the halved 16.16 value is a shift. */
        target_red = (g_battle_map_darkness_base_color.r << 15) + (red << 16);
        target_green = (g_battle_map_darkness_base_color.g << 15) + (green << 16);
        target_blue = (g_battle_map_darkness_base_color.b << 15) + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_BASE_GRAY:
        base_red = g_battle_map_darkness_base_color.r << 16;
        base_green = g_battle_map_darkness_base_color.g << 16;
        base_blue = g_battle_map_darkness_base_color.b << 16;
        gray = (base_red * 2 + base_green * 3 + base_blue) / 6;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_BASE_GRAY_HALF:
        base_red = g_battle_map_darkness_base_color.r << 16;
        base_green = g_battle_map_darkness_base_color.g << 16;
        base_blue = g_battle_map_darkness_base_color.b << 16;
        gray = (base_red * 2 + base_green * 3 + base_blue) / 12;
        target_red = gray + (red << 16);
        target_green = gray + (green << 16);
        target_blue = gray + (blue << 16);
        break;
    case BATTLE_MAP_COLOR_MODE_RESTORE:
        target_red = g_battle_map_darkness_base_color.r << 16;
        target_green = g_battle_map_darkness_base_color.g << 16;
        target_blue = g_battle_map_darkness_base_color.b << 16;
        break;
    case BATTLE_MAP_COLOR_MODE_STOP_REPEAT:
        g_battle_map_darkness_transition.repeat = BATTLE_MAP_COLOR_REPEAT_OFF;
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
        g_battle_map_darkness_transition.active = 1;
        g_battle_map_darkness_transition.phase = 0;
        g_battle_map_darkness_transition.tick = 0;
        g_battle_map_darkness_transition.period = frame_duration;
        if (mode == BATTLE_MAP_COLOR_MODE_BASE_REPEAT) {
            g_battle_map_darkness_transition.repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
            g_battle_map_darkness_transition.repeat_color.r = red;
            g_battle_map_darkness_transition.repeat_color.g = green;
            g_battle_map_darkness_transition.repeat_color.b = blue;
        }
    } else {
        g_battle_map_darkness_transition.active = 0;
        current.r = channels->red / 0x10000;
        current.g = channels->green / 0x10000;
        current.b = channels->blue / 0x10000;
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &current);
    }
}

/* Claim a free color rotation slot: the colors first_color..last_color of a
 * CLUT row rotate one place every frame_duration frames. Returns the slot index +
 * 1 as the handle, or 0 when all eight slots are in use. */
s32 battle_map_clut_start_cycle(s32 frame_duration, s32 clut_row, s32 first_color, s32 last_color) {
    s32 index;
    battle_map_clut_cycle_state_t* state;

    for (index = 0; index < BATTLE_MAP_CLUT_CYCLE_SLOT_COUNT; index++) {
        state = &g_battle_map_clut_cycle_states[index];
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

/* Stop a color rotation by the handle battle_map_clut_start_cycle
 * returned (slot index + 1). Returns 1 for a handle 1-8, else 0.
 *
 * Retail clears the active byte at handle * 10, not (handle - 1) * 10: it stops
 * the slot after the handle's own, and handle 8 clears the byte that follows the
 * eight slots, the first of g_cursor_tile_glow_cycled_palettes. */
s32 battle_map_clut_stop_cycle(s32 handle) {
    if (handle >= 1 && handle <= BATTLE_MAP_CLUT_CYCLE_SLOT_COUNT) {
        g_battle_map_clut_cycle_states[handle].active = 0;
        return 1;
    }
    return 0;
}

/* Free the eight color rotation slots (battle_map_clut_cycle_state_t) by
 * clearing their active bytes. The 32 texture animation rows of the map are not
 * touched. The byte-offset loop preserves the original address sequence;
 * indexed active fields add four bytes. */
static void battle_map_clut_reset_cycles(void) {
    s32 offset = (BATTLE_MAP_CLUT_CYCLE_SLOT_COUNT - 1) * sizeof(battle_map_clut_cycle_state_t);

    do {
        ((u8*)g_battle_map_clut_cycle_states)[offset] = 0;
        offset -= sizeof(battle_map_clut_cycle_state_t);
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
void battle_map_update_color_animations(void) {
    battle_map_palette_color_components_t saved_component;
    battle_map_background_gradient_colors_t gradient;
    battle_map_clut_cycle_state_t* state;
    battle_map_clut_row_state_t* row_state;
    battle_map_palette_color_components_t* component;
    s32 i;
    s32 j;
    s32 k;
    u16 last;
    s16 red;
    s16 green;
    s16 blue;
    u16 saved_packed;
    u16 saved_modified;
    battle_map_color_transition_t* transition;
    s32 step;

    /* Color rotations: every frame_duration frames, move each color one place and wrap the end one around. */
    for (i = 0; i < BATTLE_MAP_CLUT_CYCLE_SLOT_COUNT; i++) {
        state = &g_battle_map_clut_cycle_states[i];
        if (state->active != 0) {
            state->elapsed_frames++;
            if (state->elapsed_frames >= state->frame_duration) {
                state->elapsed_frames = 0;
                row_state = &g_battle_map_palette_state.clut_rows[state->clut_row];
                saved_component = row_state->components[state->last_color];
                last = state->last_color;
                saved_packed = row_state->packed_colors[last];
                saved_modified = g_battle_map_palette_modified_colors[state->clut_row][last];

                /* The colors move toward last_color, whichever end of the range it is. */
                if (state->first_color < last) {
                    for (k = last; state->first_color < k; k--) {
                        row_state->components[k] = row_state->components[k - 1];
                        row_state->packed_colors[k] = row_state->packed_colors[k - 1];
                        g_battle_map_palette_modified_colors[state->clut_row][k]
                            = g_battle_map_palette_modified_colors[state->clut_row][k - 1];
                    }
                } else {
                    for (k = last; k < state->first_color; k++) {
                        row_state->components[k] = row_state->components[k + 1];
                        row_state->packed_colors[k] = row_state->packed_colors[k + 1];
                        g_battle_map_palette_modified_colors[state->clut_row][k]
                            = g_battle_map_palette_modified_colors[state->clut_row][k + 1];
                    }
                }

                row_state->components[state->first_color] = saved_component;
                row_state->packed_colors[state->first_color] = saved_packed;
                g_battle_map_palette_modified_colors[state->clut_row][state->first_color] = saved_modified;
                g_battle_map_palette_state.upload_pending = 1;
            }
        }
    }

    if (g_battle_map_background_transition.active != 0) {
        battle_map_color_transition_channels_t* channels;

        channels = g_battle_map_background_transition.channels;
        for (i = 0; i < 2; i++) {
            channels[i].red += channels[i].red_step;
            channels[i].green += channels[i].green_step;
            channels[i].blue += channels[i].blue_step;
        }

        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r = channels[0].red / 0x10000;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g = channels[0].green / 0x10000;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b = channels[0].blue / 0x10000;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].r = channels[1].red / 0x10000;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].g = channels[1].green / 0x10000;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].b = channels[1].blue / 0x10000;
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &gradient);

        /* A period of 4 or more advances 32 phases, one every period / 4 frames; a shorter one
         * counts period * 8 single frames. The ambient and darkness fades below work the same way. */
        if (g_battle_map_background_transition.period >= 4) {
            if (++g_battle_map_background_transition.tick >= g_battle_map_background_transition.period >> 2) {
                g_battle_map_background_transition.tick = 0;
                if (++g_battle_map_background_transition.phase >= 0x20) {
                    /* At the end a repeating blend swaps between its tint and the restored colors; others stop. */
                    if (g_battle_map_background_transition.repeat == BATTLE_MAP_COLOR_REPEAT_TINTED) {
                        battle_map_background_blend_color(
                            BATTLE_MAP_COLOR_MODE_RESTORE, g_battle_map_background_transition.period, 0, 0, 0);
                        g_battle_map_background_transition.repeat = BATTLE_MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_battle_map_background_transition.repeat == BATTLE_MAP_COLOR_REPEAT_RESTORED) {
                        battle_map_background_blend_color(BATTLE_MAP_COLOR_MODE_BASE_REPEAT,
                            g_battle_map_background_transition.period,
                            g_battle_map_background_transition.repeat_color.r,
                            g_battle_map_background_transition.repeat_color.g,
                            g_battle_map_background_transition.repeat_color.b);
                        g_battle_map_background_transition.repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
                    } else {
                        g_battle_map_background_transition.active = 0;
                    }
                }
            }
        } else if (++g_battle_map_background_transition.phase >= g_battle_map_background_transition.period * 8) {
            if (g_battle_map_background_transition.repeat == BATTLE_MAP_COLOR_REPEAT_TINTED) {
                battle_map_background_blend_color(
                    BATTLE_MAP_COLOR_MODE_RESTORE, g_battle_map_background_transition.period, 0, 0, 0);
                g_battle_map_background_transition.repeat = BATTLE_MAP_COLOR_REPEAT_RESTORED;
            } else if (g_battle_map_background_transition.repeat == BATTLE_MAP_COLOR_REPEAT_RESTORED) {
                battle_map_background_blend_color(BATTLE_MAP_COLOR_MODE_BASE_REPEAT,
                    g_battle_map_background_transition.period, g_battle_map_background_transition.repeat_color.r,
                    g_battle_map_background_transition.repeat_color.g,
                    g_battle_map_background_transition.repeat_color.b);
                g_battle_map_background_transition.repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
            } else {
                g_battle_map_background_transition.active = 0;
            }
        }
    }

    transition = &g_battle_map_ambient_transition;
    /* Barrier: hides the constant so active and the red channel are accessed through s0. */
    __asm__("" : "=r"(transition) : "0"(transition));
    if (transition->active != 0) {
        s32 red_level;

        red_level = transition->channels.red += g_battle_map_ambient_transition.channels.red_step;
        g_battle_map_ambient_transition.channels.green += g_battle_map_ambient_transition.channels.green_step;
        g_battle_map_ambient_transition.channels.blue += g_battle_map_ambient_transition.channels.blue_step;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r = red_level / 0x10000;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g
            = g_battle_map_ambient_transition.channels.green / 0x10000;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b = g_battle_map_ambient_transition.channels.blue / 0x10000;
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, &gradient);

        if (g_battle_map_ambient_transition.period >= 4) {
            if (++g_battle_map_ambient_transition.tick >= g_battle_map_ambient_transition.period >> 2) {
                g_battle_map_ambient_transition.tick = 0;
                if (++g_battle_map_ambient_transition.phase >= 0x20) {
                    if (g_battle_map_ambient_transition.repeat == BATTLE_MAP_COLOR_REPEAT_TINTED) {
                        battle_map_ambient_blend_color(
                            BATTLE_MAP_COLOR_MODE_RESTORE, g_battle_map_ambient_transition.period, 0, 0, 0);
                        g_battle_map_ambient_transition.repeat = BATTLE_MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_battle_map_ambient_transition.repeat == BATTLE_MAP_COLOR_REPEAT_RESTORED) {
                        battle_map_ambient_blend_color(BATTLE_MAP_COLOR_MODE_BASE_REPEAT,
                            g_battle_map_ambient_transition.period, g_battle_map_ambient_transition.repeat_color.r,
                            g_battle_map_ambient_transition.repeat_color.g,
                            g_battle_map_ambient_transition.repeat_color.b);
                        g_battle_map_ambient_transition.repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
                    } else {
                        transition->active = 0;
                    }
                }
            }
        } else if (++g_battle_map_ambient_transition.phase >= g_battle_map_ambient_transition.period * 8) {
            if (g_battle_map_ambient_transition.repeat == BATTLE_MAP_COLOR_REPEAT_TINTED) {
                battle_map_ambient_blend_color(
                    BATTLE_MAP_COLOR_MODE_RESTORE, g_battle_map_ambient_transition.period, 0, 0, 0);
                g_battle_map_ambient_transition.repeat = BATTLE_MAP_COLOR_REPEAT_RESTORED;
            } else if (g_battle_map_ambient_transition.repeat == BATTLE_MAP_COLOR_REPEAT_RESTORED) {
                battle_map_ambient_blend_color(BATTLE_MAP_COLOR_MODE_BASE_REPEAT,
                    g_battle_map_ambient_transition.period, g_battle_map_ambient_transition.repeat_color.r,
                    g_battle_map_ambient_transition.repeat_color.g, g_battle_map_ambient_transition.repeat_color.b);
                g_battle_map_ambient_transition.repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
            } else {
                transition->active = 0;
            }
        }
    }

    transition = &g_battle_map_darkness_transition;
    /* Barrier: hides the constant so active and the red channel are accessed through s0. */
    __asm__("" : "=r"(transition) : "0"(transition));
    if (transition->active != 0) {
        s32 red_level;

        red_level = transition->channels.red += g_battle_map_darkness_transition.channels.red_step;
        g_battle_map_darkness_transition.channels.green += g_battle_map_darkness_transition.channels.green_step;
        g_battle_map_darkness_transition.channels.blue += g_battle_map_darkness_transition.channels.blue_step;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r = red_level / 0x10000;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g
            = g_battle_map_darkness_transition.channels.green / 0x10000;
        gradient.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b
            = g_battle_map_darkness_transition.channels.blue / 0x10000;
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &gradient);

        if (g_battle_map_darkness_transition.period >= 4) {
            if (++g_battle_map_darkness_transition.tick >= g_battle_map_darkness_transition.period >> 2) {
                g_battle_map_darkness_transition.tick = 0;
                if (++g_battle_map_darkness_transition.phase >= 0x20) {
                    if (g_battle_map_darkness_transition.repeat == BATTLE_MAP_COLOR_REPEAT_TINTED) {
                        battle_map_darkness_blend_color(
                            BATTLE_MAP_COLOR_MODE_RESTORE, g_battle_map_darkness_transition.period, 0, 0, 0);
                        g_battle_map_darkness_transition.repeat = BATTLE_MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_battle_map_darkness_transition.repeat == BATTLE_MAP_COLOR_REPEAT_RESTORED) {
                        battle_map_darkness_blend_color(BATTLE_MAP_COLOR_MODE_BASE_REPEAT,
                            g_battle_map_darkness_transition.period, g_battle_map_darkness_transition.repeat_color.r,
                            g_battle_map_darkness_transition.repeat_color.g,
                            g_battle_map_darkness_transition.repeat_color.b);
                        g_battle_map_darkness_transition.repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
                    } else {
                        transition->active = 0;
                    }
                }
            }
        } else if (++g_battle_map_darkness_transition.phase >= g_battle_map_darkness_transition.period * 8) {
            if (g_battle_map_darkness_transition.repeat == BATTLE_MAP_COLOR_REPEAT_TINTED) {
                battle_map_darkness_blend_color(
                    BATTLE_MAP_COLOR_MODE_RESTORE, g_battle_map_darkness_transition.period, 0, 0, 0);
                g_battle_map_darkness_transition.repeat = BATTLE_MAP_COLOR_REPEAT_RESTORED;
            } else if (g_battle_map_darkness_transition.repeat == BATTLE_MAP_COLOR_REPEAT_RESTORED) {
                battle_map_darkness_blend_color(BATTLE_MAP_COLOR_MODE_BASE_REPEAT,
                    g_battle_map_darkness_transition.period, g_battle_map_darkness_transition.repeat_color.r,
                    g_battle_map_darkness_transition.repeat_color.g, g_battle_map_darkness_transition.repeat_color.b);
                g_battle_map_darkness_transition.repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
            } else {
                transition->active = 0;
            }
        }

        /* On maps that ignore weather, color 0 of the first map CLUT follows the darkness color: 16.16
         * channels cut to five bits, with the STP bit set so the color is not transparent. */
        if (g_weather_flags & BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER) {
            red = g_battle_map_darkness_transition.channels.red / 0x80000;
            green = g_battle_map_darkness_transition.channels.green / 0x80000;
            blue = g_battle_map_darkness_transition.channels.blue / 0x80000;
            g_battle_map_palette_modified_colors[0][0] = red + (green << 5) + (blue << 10) + 0x8000;

            g_battle_map_palette_state.upload_pending = 1;
        }
    }

    /* Rows with `blend_as_row` clear (sprites, units, shadows, action results) blend each of their 16 CLUTs
     * on its own; the other rows blend all 256 colors under animation 0. */
    for (i = 0; i < BATTLE_MAP_CLUT_ROW_COUNT; i++) {
        if (g_battle_map_palette_state.clut_rows[i].blend_as_row == 0) {
            for (k = 0; k < BATTLE_MAP_CLUTS_PER_ROW; k++) {
                if (g_battle_map_palette_state.clut_rows[i].animations[k].active == 0) {
                    continue;
                }

                /* Mode 4 and up takes one of 32 blend steps every mode / 4 frames;
                 * lower modes take one of 8 each frame. */
                if (g_battle_map_palette_state.clut_rows[i].animations[k].mode >= 4) {
                    if (++g_battle_map_palette_state.clut_rows[i].animations[k].delay_counter
                        >= g_battle_map_palette_state.clut_rows[i].animations[k].mode >> 2) {
                        step = g_battle_map_palette_state.clut_rows[i].animations[k].blend_step;
                        g_battle_map_palette_state.clut_rows[i].animations[k].delay_counter = 0;
                        component
                            = &g_battle_map_palette_state.clut_rows[i].components[k * BATTLE_MAP_CLUT_COLOR_COUNT];
                        for (j = 0; j < BATTLE_MAP_CLUT_COLOR_COUNT; j++) {
                            /* Black colors are left alone, and a blend that reaches black stores blue 1,
                             * so a color never becomes the transparent value 0. */
                            if (component[j].color.r + component[j].color.g + component[j].color.b != 0) {
                                red = component[j].color.r
                                    + g_battle_map_palette_blend_steps_32[component[j].delta_biased.r][step];
                                green = component[j].color.g
                                    + g_battle_map_palette_blend_steps_32[component[j].delta_biased.g][step];
                                blue = component[j].color.b
                                    + g_battle_map_palette_blend_steps_32[component[j].delta_biased.b][step];
                                if ((red | green | blue) == 0) {
                                    blue = 1;
                                }
                                component[j].color.b = blue;
                                component[j].color.r = red;
                                component[j].color.g = green;
                                g_battle_map_palette_modified_colors[i][k * BATTLE_MAP_CLUT_COLOR_COUNT + j]
                                    = red + (green << 5) + (blue << 10) + (component[j].semi_transparent << 15);
                            }
                        }

                        if (++g_battle_map_palette_state.clut_rows[i].animations[k].blend_step
                            >= BATTLE_MAP_PALETTE_BLEND_STEP_COUNT) {
                            if (g_battle_map_palette_state.clut_rows[i].animations[k].repeat
                                == BATTLE_MAP_COLOR_REPEAT_TINTED) {
                                battle_map_palette_modify(BATTLE_MAP_COLOR_MODE_RESTORE,
                                    g_battle_map_palette_state.clut_rows[i].animations[k].mode, i, k, 0, 0, 0, 0);
                                g_battle_map_palette_state.clut_rows[i].animations[k].repeat
                                    = BATTLE_MAP_COLOR_REPEAT_RESTORED;
                            } else if (g_battle_map_palette_state.clut_rows[i].animations[k].repeat
                                == BATTLE_MAP_COLOR_REPEAT_RESTORED) {
                                battle_map_palette_modify(BATTLE_MAP_COLOR_MODE_BASE_REPEAT,
                                    g_battle_map_palette_state.clut_rows[i].animations[k].mode, i, k, 0,
                                    g_battle_map_palette_state.clut_rows[i].animations[k].repeat_color.r,
                                    g_battle_map_palette_state.clut_rows[i].animations[k].repeat_color.g,
                                    g_battle_map_palette_state.clut_rows[i].animations[k].repeat_color.b);
                                g_battle_map_palette_state.clut_rows[i].animations[k].repeat
                                    = BATTLE_MAP_COLOR_REPEAT_TINTED;
                            } else {
                                g_battle_map_palette_state.clut_rows[i].animations[k].active = 0;
                            }
                        }
                    }
                } else {
                    step = g_battle_map_palette_state.clut_rows[i].animations[k].blend_step;
                    component = &g_battle_map_palette_state.clut_rows[i].components[k * BATTLE_MAP_CLUT_COLOR_COUNT];
                    for (j = 0; j < BATTLE_MAP_CLUT_COLOR_COUNT; j++) {
                        if (component[j].color.r + component[j].color.g + component[j].color.b != 0) {
                            red = component[j].color.r
                                + g_battle_map_palette_blend_steps_8[component[j].delta_biased.r][step];
                            green = component[j].color.g
                                + g_battle_map_palette_blend_steps_8[component[j].delta_biased.g][step];
                            blue = component[j].color.b
                                + g_battle_map_palette_blend_steps_8[component[j].delta_biased.b][step];
                            if ((red | green | blue) == 0) {
                                blue = 1;
                            }
                            component[j].color.b = blue;
                            component[j].color.r = red;
                            component[j].color.g = green;
                            g_battle_map_palette_modified_colors[i][k * BATTLE_MAP_CLUT_COLOR_COUNT + j]
                                = red + (green << 5) + (blue << 10) + (component[j].semi_transparent << 15);
                        }
                    }

                    if (++g_battle_map_palette_state.clut_rows[i].animations[k].blend_step
                        >= BATTLE_MAP_PALETTE_FAST_BLEND_STEP_COUNT) {
                        if (g_battle_map_palette_state.clut_rows[i].animations[k].repeat
                            == BATTLE_MAP_COLOR_REPEAT_TINTED) {
                            battle_map_palette_modify(BATTLE_MAP_COLOR_MODE_RESTORE,
                                g_battle_map_palette_state.clut_rows[i].animations[k].mode, i, k, 0, 0, 0, 0);
                            g_battle_map_palette_state.clut_rows[i].animations[k].repeat
                                = BATTLE_MAP_COLOR_REPEAT_RESTORED;
                        } else if (g_battle_map_palette_state.clut_rows[i].animations[k].repeat
                            == BATTLE_MAP_COLOR_REPEAT_RESTORED) {
                            battle_map_palette_modify(BATTLE_MAP_COLOR_MODE_BASE_REPEAT,
                                g_battle_map_palette_state.clut_rows[i].animations[k].mode, i, k, 0,
                                g_battle_map_palette_state.clut_rows[i].animations[k].repeat_color.r,
                                g_battle_map_palette_state.clut_rows[i].animations[k].repeat_color.g,
                                g_battle_map_palette_state.clut_rows[i].animations[k].repeat_color.b);
                            g_battle_map_palette_state.clut_rows[i].animations[k].repeat
                                = BATTLE_MAP_COLOR_REPEAT_TINTED;
                        } else {
                            g_battle_map_palette_state.clut_rows[i].animations[k].active = 0;
                        }
                    }
                }
                g_battle_map_palette_state.upload_pending = 1;
            }
        } else if (g_battle_map_palette_state.clut_rows[i].animations[0].active != 0) {
            if (g_battle_map_palette_state.clut_rows[i].animations[0].mode >= 4) {
                if (++g_battle_map_palette_state.clut_rows[i].animations[0].delay_counter
                    >= g_battle_map_palette_state.clut_rows[i].animations[0].mode >> 2) {
                    step = g_battle_map_palette_state.clut_rows[i].animations[0].blend_step;
                    g_battle_map_palette_state.clut_rows[i].animations[0].delay_counter = 0;
                    component = g_battle_map_palette_state.clut_rows[i].components;
                    for (j = 0; j < BATTLE_MAP_CLUT_ROW_COLOR_COUNT; j++) {
                        if (component[j].color.r + component[j].color.g + component[j].color.b != 0) {
                            red = component[j].color.r
                                + g_battle_map_palette_blend_steps_32[component[j].delta_biased.r][step];
                            green = component[j].color.g
                                + g_battle_map_palette_blend_steps_32[component[j].delta_biased.g][step];
                            blue = component[j].color.b
                                + g_battle_map_palette_blend_steps_32[component[j].delta_biased.b][step];
                            if ((red | green | blue) == 0) {
                                blue = 1;
                            }
                            component[j].color.r = red;
                            component[j].color.g = green;
                            component[j].color.b = blue;
                            g_battle_map_palette_modified_colors[i][j]
                                = red + (green << 5) + (blue << 10) + (component[j].semi_transparent << 15);
                        }
                    }

                    if (++g_battle_map_palette_state.clut_rows[i].animations[0].blend_step
                        >= BATTLE_MAP_PALETTE_BLEND_STEP_COUNT) {
                        if (g_battle_map_palette_state.clut_rows[i].animations[0].repeat
                            == BATTLE_MAP_COLOR_REPEAT_TINTED) {
                            battle_map_palette_modify(BATTLE_MAP_COLOR_MODE_RESTORE,
                                g_battle_map_palette_state.clut_rows[i].animations[0].mode, i, 0, 1, 0, 0, 0);
                            g_battle_map_palette_state.clut_rows[i].animations[0].repeat
                                = BATTLE_MAP_COLOR_REPEAT_RESTORED;
                        } else if (g_battle_map_palette_state.clut_rows[i].animations[0].repeat
                            == BATTLE_MAP_COLOR_REPEAT_RESTORED) {
                            battle_map_palette_modify(BATTLE_MAP_COLOR_MODE_BASE_REPEAT,
                                g_battle_map_palette_state.clut_rows[i].animations[0].mode, i, 0, 1,
                                g_battle_map_palette_state.clut_rows[i].animations[0].repeat_color.r,
                                g_battle_map_palette_state.clut_rows[i].animations[0].repeat_color.g,
                                g_battle_map_palette_state.clut_rows[i].animations[0].repeat_color.b);
                            g_battle_map_palette_state.clut_rows[i].animations[0].repeat
                                = BATTLE_MAP_COLOR_REPEAT_TINTED;
                        } else {
                            for (j = BATTLE_MAP_CLUTS_PER_ROW - 1; j >= 0; j--) {
                                g_battle_map_palette_state.clut_rows[i].animations[j].active = 0;
                            }
                        }
                    }
                }
            } else {
                step = g_battle_map_palette_state.clut_rows[i].animations[0].blend_step;
                component = g_battle_map_palette_state.clut_rows[i].components;
                for (j = 0; j < BATTLE_MAP_CLUT_ROW_COLOR_COUNT; j++) {
                    if (component[j].color.r + component[j].color.g + component[j].color.b != 0) {
                        red = component[j].color.r
                            + g_battle_map_palette_blend_steps_8[component[j].delta_biased.r][step];
                        green = component[j].color.g
                            + g_battle_map_palette_blend_steps_8[component[j].delta_biased.g][step];
                        blue = component[j].color.b
                            + g_battle_map_palette_blend_steps_8[component[j].delta_biased.b][step];
                        if ((red | green | blue) == 0) {
                            blue = 1;
                        }
                        component[j].color.r = red;
                        component[j].color.g = green;
                        component[j].color.b = blue;
                        g_battle_map_palette_modified_colors[i][j]
                            = red + (green << 5) + (blue << 10) + (component[j].semi_transparent << 15);
                    }
                }

                if (++g_battle_map_palette_state.clut_rows[i].animations[0].blend_step
                    >= BATTLE_MAP_PALETTE_FAST_BLEND_STEP_COUNT) {
                    if (g_battle_map_palette_state.clut_rows[i].animations[0].repeat
                        == BATTLE_MAP_COLOR_REPEAT_TINTED) {
                        battle_map_palette_modify(BATTLE_MAP_COLOR_MODE_RESTORE,
                            g_battle_map_palette_state.clut_rows[i].animations[0].mode, i, 0, 1, 0, 0, 0);
                        g_battle_map_palette_state.clut_rows[i].animations[0].repeat = BATTLE_MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_battle_map_palette_state.clut_rows[i].animations[0].repeat
                        == BATTLE_MAP_COLOR_REPEAT_RESTORED) {
                        battle_map_palette_modify(BATTLE_MAP_COLOR_MODE_BASE_REPEAT,
                            g_battle_map_palette_state.clut_rows[i].animations[0].mode, i, 0, 1,
                            g_battle_map_palette_state.clut_rows[i].animations[0].repeat_color.r,
                            g_battle_map_palette_state.clut_rows[i].animations[0].repeat_color.g,
                            g_battle_map_palette_state.clut_rows[i].animations[0].repeat_color.b);
                        g_battle_map_palette_state.clut_rows[i].animations[0].repeat = BATTLE_MAP_COLOR_REPEAT_TINTED;
                    } else {
                        for (j = BATTLE_MAP_CLUTS_PER_ROW - 1; j >= 0; j--) {
                            g_battle_map_palette_state.clut_rows[i].animations[j].active = 0;
                        }
                    }
                }
            }

            /* Put color 0 back on the darkness color after the blend rewrote it. */
            if (i == 0 && (g_weather_flags & BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER)) {
                red = g_battle_map_darkness_transition.channels.red / 0x80000;
                green = g_battle_map_darkness_transition.channels.green / 0x80000;
                blue = g_battle_map_darkness_transition.channels.blue / 0x80000;
                g_battle_map_palette_modified_colors[0][0] = red + (green << 5) + (blue << 10) + 0x8000;
            }

            g_battle_map_palette_state.upload_pending = 1;
        }
    }
}

/* Stop the blend of CLUT clut_index of a row, or of all sixteen when stop_all is
 * 1. The colors stay as blended so far. */
void battle_map_palette_stop_animation(s32 clut_row, s32 clut_index, s32 stop_all) {
    s32 index;

    if (stop_all == 1) {
        for (index = BATTLE_MAP_CLUTS_PER_ROW - 1; index >= 0; index--) {
            g_battle_map_palette_state.clut_rows[clut_row].animations[index].active = 0;
        }
    } else {
        g_battle_map_palette_state.clut_rows[clut_row].animations[clut_index].active = 0;
    }
}

/* Load the sixteen colors of one CLUT into a map CLUT row: the upload staging
 * (g_battle_map_palette_modified_colors), the row's packed mirror, and its
 * decoded 5-bit components and alpha bit. */
static void battle_map_palette_extract_colors(const palette_t palette, s32 clut_row, s32 clut_index) {
    s32 color_index;
    s32 selected_clut = clut_index;
    s32 i = 0;
    const u16* palette_ptr;
    u16* upload_palette_base;
    u16* upload_row;
    u16* packed_row;
    battle_map_palette_color_components_t* component_row;
    u16 color;
    battle_map_palette_color_components_t* components;

    component_row = g_battle_map_palette_state.clut_rows[clut_row].components;
    upload_palette_base = g_battle_map_palette_modified_colors[0];
    upload_row = upload_palette_base + clut_row * BATTLE_MAP_CLUT_ROW_COLOR_COUNT;
    packed_row = g_battle_map_palette_state.clut_rows[clut_row].packed_colors;
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
    } while (i < BATTLE_MAP_CLUT_COLOR_COUNT);
}

/* Load colors into a map CLUT row and mark the rows for upload: the row's
 * sixteen CLUTs (256 colors) when load_all is 1, otherwise the sixteen colors
 * of CLUT clut_index. The blends of the loaded CLUTs stop. Nothing happens while
 * palette loading is disabled for the row. */
void battle_map_palette_load_data(const palette_t* palettes, s32 clut_row, s32 clut_index, s32 load_all) {
    s32 clear_index;
    s32 i;

    if (g_battle_map_palette_state.clut_rows[clut_row].load_disabled != 0) {
        return;
    }

    if (load_all == 1) {
        for (clear_index = BATTLE_MAP_CLUTS_PER_ROW - 1; clear_index >= 0; clear_index--) {
            g_battle_map_palette_state.clut_rows[clut_row].animations[clear_index].active = 0;
        }

        for (i = 0; i < BATTLE_MAP_CLUTS_PER_ROW; i++) {
            battle_map_palette_extract_colors(palettes[i], clut_row, i);
        }
    } else {
        g_battle_map_palette_state.clut_rows[clut_row].animations[clut_index].active = 0;
        battle_map_palette_extract_colors(*palettes, clut_row, clut_index);
    }

    g_battle_map_palette_state.upload_pending = 1;
}

/* Tint the colors at source into a CLUT row: all 256 when all_colors is 1, otherwise CLUT clut_index.
 * Each nonzero color takes the offsets added to itself, its half or its gray (mode), clamped to
 * 0..31 and kept from becoming transparent black. The results become the row's base colors;
 * update_current_colors also stages them for upload and sets the components of CLUTs that are not blending. */
void battle_map_palette_modify_from_other(s32 mode, const rgba5551_t* source, s32 clut_row, s32 clut_index,
    s32 all_colors, s16 red, s16 green, s16 blue, s32 update_current_colors) {
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
    battle_map_palette_color_components_t* component;

    /* Named component reads change the masks, frame and register allocation. */
    if (all_colors == 1) {
        i = 0;
        do {
            color = *(const u16*)source;
            if (*(const s16*)source != 0) {
                switch (mode) {
                case BATTLE_MAP_COLOR_MODE_BASE_HALF:
                    red_part = color & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green = green + ((color & 0x3e0) >> 6);
                    out_blue = blue + ((color & 0x7c00) >> 11);
                    break;
                case BATTLE_MAP_COLOR_MODE_BASE_GRAY:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case BATTLE_MAP_COLOR_MODE_BASE_GRAY_HALF:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case BATTLE_MAP_COLOR_MODE_BASE:
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
            if (update_current_colors != 0) {
                g_battle_map_palette_modified_colors[clut_row][i]
                    = out_red + (out_green << 5) + (out_blue << 10) + (semi_transparent << 15);
                if (g_battle_map_palette_state.clut_rows[clut_row].animations[i / BATTLE_MAP_CLUT_COLOR_COUNT].active
                    == 0) {
                    component = &g_battle_map_palette_state.clut_rows[clut_row].components[i];
                    component->color.r = out_red;
                    component->color.g = out_green;
                    component->color.b = out_blue;
                    component->semi_transparent = semi_transparent;
                }
            }
            g_battle_map_palette_state.clut_rows[clut_row].packed_colors[i]
                = out_red + (out_green << 5) + (out_blue << 10) + (semi_transparent << 15);
            i++;
            source++;
        } while (i < BATTLE_MAP_CLUT_ROW_COLOR_COUNT);
    } else {
        i = 0;
        do {
            color = *(const u16*)source;
            if (*(const s16*)source != 0) {
                switch (mode) {
                case BATTLE_MAP_COLOR_MODE_BASE_HALF:
                    red_part = color & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green = green + ((color & 0x3e0) >> 6);
                    out_blue = blue + ((color & 0x7c00) >> 11);
                    break;
                case BATTLE_MAP_COLOR_MODE_BASE_GRAY:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case BATTLE_MAP_COLOR_MODE_BASE_GRAY_HALF:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case BATTLE_MAP_COLOR_MODE_BASE:
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
            if (update_current_colors != 0) {
                index = clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i;
                g_battle_map_palette_modified_colors[clut_row][index]
                    = out_red + (out_green << 5) + (out_blue << 10) + (semi_transparent << 15);
                if (g_battle_map_palette_state.clut_rows[clut_row].animations[clut_index].active == 0) {
                    component = &g_battle_map_palette_state.clut_rows[clut_row].components[index];
                    component->color.r = out_red;
                    component->color.g = out_green;
                    component->color.b = out_blue;
                    component->semi_transparent = semi_transparent;
                }
            }
            g_battle_map_palette_state.clut_rows[clut_row].packed_colors[clut_index * BATTLE_MAP_CLUT_COLOR_COUNT + i]
                = out_red + (out_green << 5) + (out_blue << 10) + (semi_transparent << 15);
            i++;
            source++;
        } while (i < BATTLE_MAP_CLUT_COLOR_COUNT);
    }
    g_battle_map_palette_state.upload_pending = 1;
}

/* Write one CLUT of a row (the whole row when all_colors is 1; retail callers pass 0)
 * from the colors at `source` with the red, green and blue offsets added:
 * battle_map_palette_modify_from_other in BATTLE_MAP_COLOR_MODE_RESTORE, a mode in which it
 * neither halves nor grays the colors. */
void battle_map_palette_update_from_other(const rgba5551_t* source, s32 clut_row, s32 clut_index, s32 all_colors,
    s16 red, s16 green, s16 blue, s32 update_current_colors) {
    battle_map_palette_modify_from_other(BATTLE_MAP_COLOR_MODE_RESTORE, source, clut_row, clut_index, all_colors, red,
        green, blue, update_current_colors);
}

/* Load the two map background gradient colors for the current map. */
void battle_map_background_init(void) {
    battle_map_background_gradient_colors_t* colors;

    colors = battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT, colors);
    g_battle_map_background_transition.colors[0] = colors->colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP];
    g_battle_map_background_transition.colors[1] = colors->colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM];
    g_battle_map_background_transition.channels[0].red = colors->colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r << 16;
    g_battle_map_background_transition.channels[0].green = colors->colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g << 16;
    g_battle_map_background_transition.channels[0].blue = colors->colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b << 16;
    g_battle_map_background_transition.channels[1].red = colors->colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].r << 16;
    g_battle_map_background_transition.channels[1].green = colors->colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].g
        << 16;
    g_battle_map_background_transition.channels[1].blue = colors->colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].b << 16;
}

/* Seed the ambient light fade from the color the map loaded: the base color and
 * the 16.16 transition channels take the current GTE back color.
 *
 * The unset `color` is passed as the command's data argument, which the GET
 * command does not read, and then receives the result. */
void battle_map_ambient_init(void) {
    CVECTOR* color;

    color = battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR, color);
    g_battle_map_ambient_base_color = *color;
    g_battle_map_ambient_transition.channels.red = color->r << 16;
    g_battle_map_ambient_transition.channels.green = color->g << 16;
    g_battle_map_ambient_transition.channels.blue = color->b << 16;
}

/* Seed the darkness fade from the map's current darkness color: the base color
 * and the 16.16 transition channels.
 *
 * The unset `color` is passed as the command's data argument, which the GET
 * command does not read, and then receives the result. */
void battle_map_darkness_init(void) {
    CVECTOR* color;

    color = battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR, color);
    g_battle_map_darkness_base_color = *color;
    g_battle_map_darkness_transition.channels.red = color->r << 16;
    g_battle_map_darkness_transition.channels.green = color->g << 16;
    g_battle_map_darkness_transition.channels.blue = color->b << 16;
}

/* Upload the pending CLUT rows as one image to the lines below the rows, and
 * on the next call move them into place. */
void battle_map_clut_upload_rows(void) {
    RECT rectangle;
    s32* move_pending;

    move_pending = &g_battle_map_palette_state.move_pending;

    if (*move_pending != 0) {
        rectangle.x = 0;
        rectangle.y = BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_COUNT;
        rectangle.w = BATTLE_MAP_CLUT_ROW_COLOR_COUNT;
        rectangle.h = BATTLE_MAP_CLUT_ROW_COUNT;
        MoveImage(&rectangle, 0, BATTLE_MAP_CLUT_ROWS_Y);
        *move_pending = 0;
    }

    if (g_battle_map_palette_state.upload_pending != 0) {
        rectangle.x = 0;
        rectangle.y = BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_COUNT;
        rectangle.w = BATTLE_MAP_CLUT_ROW_COLOR_COUNT;
        rectangle.h = BATTLE_MAP_CLUT_ROW_COUNT;
        LoadImage(&rectangle, (u32*)g_battle_map_palette_modified_colors);
        g_battle_map_palette_state.upload_pending = 0;
        *move_pending = 1;
    }
}

/* Reset the map palette state at step 0 of the map init sequences: no upload or
 * move pending, every row open to updates with its sixteen CLUT animations
 * stopped, the background gradient and ambient light fades and their repeats off,
 * and the eight color rotation slots free.
 *
 * Each row's `blend_as_row` is set, which makes battle_map_update_color_animations blend the
 * row's 256 colors as one under animation 0. The sprite, unit, unit shadow and
 * action result rows get 0 and blend each of their sixteen CLUTs on its own. Of
 * the stored deltas only the first color's red delta of each row is cleared. */
void battle_map_palette_init_state(void) {
    s32 clut_row;
    s32 animation_index;

    g_battle_map_palette_state.upload_pending = 0;
    g_battle_map_palette_state.move_pending = 0;

    for (clut_row = 0; clut_row < BATTLE_MAP_CLUT_ROW_COUNT; clut_row++) {
        g_battle_map_palette_state.clut_rows[clut_row].load_disabled = 0;
        g_battle_map_palette_state.clut_rows[clut_row].blend_as_row = 1;

        for (animation_index = BATTLE_MAP_CLUTS_PER_ROW - 1; animation_index >= 0; animation_index--) {
            g_battle_map_palette_state.clut_rows[clut_row].animations[animation_index].active = 0;
        }

        g_battle_map_palette_state.clut_rows[clut_row].components[0].delta_biased.r = 0;
    }

    g_battle_map_background_transition.active = 0;
    g_battle_map_background_transition.repeat = BATTLE_MAP_COLOR_REPEAT_OFF;
    g_battle_map_ambient_transition.active = 0;
    g_battle_map_ambient_transition.repeat = BATTLE_MAP_COLOR_REPEAT_OFF;
    g_battle_map_palette_state.clut_rows[BATTLE_MAP_CLUT_ROW_SPRITES].blend_as_row = 0;
    g_battle_map_palette_state.clut_rows[BATTLE_MAP_CLUT_ROW_UNITS].blend_as_row = 0;
    g_battle_map_palette_state.clut_rows[BATTLE_MAP_CLUT_ROW_UNIT_SHADOWS].blend_as_row = 0;
    g_battle_map_palette_state.clut_rows[BATTLE_MAP_CLUT_ROW_ACTION_RESULTS].blend_as_row = 0;
    battle_map_clut_reset_cycles();
}

/* Pause every map texture animation (event MapFreeze 0x89, also effect scripts);
 * battle_map_unfreeze resumes them. */
void battle_map_freeze(void) {
    battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}

/* Resume the map texture animations paused by map_freeze. */
void battle_map_unfreeze(void) {
    battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}

/* Event ColorField: tint or fade all 256 colors of the map CLUT row with
 * battle_map_palette_modify. `mode` is its color mode (battle_map_color_mode_e) and
 * `amount` 0 applies the colors at once, another value fades to them. */
void battle_map_color_field(s32 mode, s32 amount, s16 red, s16 green, s16 blue) {
    battle_map_palette_modify(mode, amount, BATTLE_MAP_CLUT_ROW_MAP, 0, 1, red, green, blue);
}

/* Tints one unit's two CLUTs through battle_map_palette_modify with color mode
 * `mode` (battle_map_color_mode_e) and `amount` (0 applies at once, else a blend).
 *
 * The unit's CLUT of BATTLE_MAP_CLUT_ROW_UNITS takes the offsets as given. Its CLUT of
 * BATTLE_MAP_CLUT_ROW_UNIT_SHADOWS takes the same offsets while the unit stands on a
 * tile with depth, at or below the tile's surface, and the negated offsets
 * otherwise. An absent unit is skipped. */
static void sprite_palette_apply_unit_modulation(s32 mode, s32 amount, s32 runtime_id, s16 red, s16 green, s16 blue) {
    SVECTOR world_coordinates;
    unit_t* unit;
    battle_map_tile_t* tile;

    unit = unit_get_by_runtime_id(runtime_id);
    if (unit == 0) {
        return;
    }

    unit_copy_draw_coords(&world_coordinates, unit);
    tile = battle_map_tile_get_pointer(
        world_coordinates.vx / BATTLE_MAP_TILE_SIZE, world_coordinates.vz / BATTLE_MAP_TILE_SIZE, unit->map_z);

    battle_map_palette_modify(mode, amount, BATTLE_MAP_CLUT_ROW_UNITS, runtime_id, 0, red, green, blue);

    if (tile->depth_half_height.depth != 0
        && unit->world_coords.vy >= -(tile->height + tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP) {
        battle_map_palette_modify(mode, amount, BATTLE_MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0, red, green, blue);
    } else {
        battle_map_palette_modify(mode, amount, BATTLE_MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0, -red, -green, -blue);
    }
}

/* sprite_palette_apply_unit_modulation for one unit, or for all sixteen
 * when runtime_id is 16 or more. */
void sprite_palette_apply_modulation_to_units(s32 mode, s32 amount, s32 runtime_id, s16 red, s16 green, s16 blue) {
    s32 index;

    if (runtime_id >= UNIT_RUNTIME_SLOT_COUNT) {
        for (index = 0; index < UNIT_RUNTIME_SLOT_COUNT; index++) {
            sprite_palette_apply_unit_modulation(mode, amount, index, red, green, blue);
        }
    } else {
        sprite_palette_apply_unit_modulation(mode, amount, runtime_id, red, green, blue);
    }
}

/* Event ColorBGBeta (0x31): battle_map_background_blend_color with the color offsets
 * narrowed to s16. */
void battle_map_background_modify(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue) {
    battle_map_background_blend_color(mode, frame_duration, red, green, blue);
}

/* battle_map_ambient_blend_color with the color offsets narrowed to s16:
 * the entry event MapDarkness calls. */
void battle_map_ambient_start_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue) {
    battle_map_ambient_blend_color(mode, frame_duration, red, green, blue);
}

/* battle_map_darkness_blend_color with the color offsets narrowed to s16. */
void battle_map_darkness_start_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue) {
    battle_map_darkness_blend_color(mode, frame_duration, red, green, blue);
}

/* battle_map_palette_update_colors for all sixteen CLUTs of the map row. */
void battle_map_palette_update_all_from_color_data(s32 mode, const clut_t color_data) {
    battle_map_palette_update_colors(mode, BATTLE_MAP_CLUT_ROW_MAP, 0, 1, color_data);
}

/* battle_map_palette_update_colors for one CLUT of the unit row. A clut_index of
 * 16 or more applies the same sixteen colors to every unit CLUT. */
void battle_map_palette_update_colors_by_index(s32 mode, s32 clut_index, const palette_t* color_data) {
    s32 index;

    if (clut_index >= BATTLE_MAP_CLUTS_PER_ROW) {
        for (index = 0; index < BATTLE_MAP_CLUTS_PER_ROW; index++) {
            battle_map_palette_update_colors(mode, BATTLE_MAP_CLUT_ROW_UNITS, index, 0, color_data);
        }
    } else {
        battle_map_palette_update_colors(mode, BATTLE_MAP_CLUT_ROW_UNITS, clut_index, 0, color_data);
    }
}

/* Fade the background gradient to `colors` without changing the base pair, unlike
 * battle_map_background_transition_and_store. */
void battle_map_background_set(s32 frame_duration, const battle_map_background_gradient_colors_t* colors) {
    battle_map_background_transition(frame_duration, colors);
}

/* Fade the ambient light to `color` without changing the base color, unlike
 * battle_map_ambient_transition_and_store. */
void battle_map_ambient_set(s32 frame_duration, const CVECTOR* color) {
    battle_map_ambient_transition(frame_duration, color);
}

/* Fade the map darkness to `color` without changing the base color, unlike
 * battle_map_darkness_transition_and_store. */
void battle_map_darkness_set(s32 frame_duration, const CVECTOR* color) {
    battle_map_darkness_transition(frame_duration, color);
}

/* Read pad 1 for the battle loop: derive the pressed and released buttons and the stable-frame
 * count, keeping last frame's values in the input copies. Triangle requests the status menu on a
 * turn the player does not control; Start+Select+L1+R1 resets the game. Always returns 1. */
s32 state_update_controller_input(void) {
    u32 pressed;
    u32 previous;
    u32 current;
    u32 released;
    unit_t* unit;

    g_input_current_buttons = PadRead(1);
    if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
        script_filter_controller_input(&g_input_current_buttons);
    }

    pressed = g_input_pressed_buttons;
    previous = g_input_previous_buttons;
    current = g_input_current_buttons;
    g_input_pressed_copy_0 = pressed;
    g_input_pressed_copy_1 = pressed;
    g_input_pressed_copy_2 = pressed;
    g_input_pressed_copy_3 = pressed;
    g_input_pressed_buttons = ~previous & current;
    released = ~current & previous;
    g_input_previous_copy_0 = previous;
    g_input_previous_copy_1 = previous;
    g_input_previous_copy_2 = previous;
    g_input_previous_copy_3 = previous;
    g_input_released_buttons = released;

    if (previous == current) {
        g_input_stable_frames++;
    } else {
        g_input_stable_frames = 0;
    }

    g_input_previous_buttons = g_input_current_buttons;
    if ((g_input_pressed_buttons & PSX_PAD_TRIANGLE) != 0 && g_game_state != GAME_STATE_EVENT) {
        unit = unit_get_source();
        if (g_menu_status_enabled != 0 && g_menu_status_requested == 0
            && (unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) == 0) {
            sound_play_sfx(SOUND_SFX_STATUS_MENU_REQUEST);
            g_menu_status_requested = 1;
        }
    }

    if ((g_input_previous_buttons & INPUT_RESET_BUTTONS) == INPUT_RESET_BUTTONS) {
        system_reset_game();
    }
    return 1;
}

/* Read pad 1 for the deployment screen and derive the pressed and released
 * buttons; the reset button combination resets the game. Always returns 1, so the
 * -1 that state_run_map_load tests for does not occur. */
s32 deploy_update_controller_input(void) {
    u32 previous;
    u32 current = PadRead(1);

    previous = g_input_previous_buttons;
    g_input_current_buttons = current;
    g_input_previous_buttons = current;
    g_input_pressed_buttons = ~previous & current;
    g_input_released_buttons = ~current & previous;

    if ((current & INPUT_RESET_BUTTONS) == INPUT_RESET_BUTTONS) {
        system_reset_game();
    }
    return 1;
}

/* Reset the GPU and set up the two frame buffers for a width x height battle
 * screen with the given projection distance and background color.
 *
 * The second buffer lies at y 0xf0 unless the height is 0x1e0. The draw
 * environments start at x -0x80, so primitive x 0x80 is the left screen edge;
 * dithering is on and the automatic background clear is off. The area of
 * g_deploy_clear_rect is cleared to black and, when font printing is
 * enabled, debug_load_efc_font loads the print font again.
 * state_run_map_load calls this with 256 x 240 and projection 0x200. */
void deploy_init_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue) {
    RECT clear_rect = g_deploy_clear_rect;
    s32 second_y = -(height != 0x1e0) & 0xf0;

    SetDispMask(0);
    ResetGraph(1);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_gfx_draw_envs[0], -GFX_BATTLE_SCREEN_X, 0, width + GFX_BATTLE_SCREEN_X, height);
    SetDefDispEnv(&g_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_gfx_draw_envs[1], -GFX_BATTLE_SCREEN_X, second_y, width + GFX_BATTLE_SCREEN_X, height);
    SetDefDispEnv(&g_gfx_display_envs[1], 0, 0, width, height);

    g_gfx_draw_envs[0].dtd = g_gfx_draw_envs[1].dtd = 1;
    g_gfx_draw_envs[0].isbg = g_gfx_draw_envs[1].isbg = 0;
    setRGB0(&g_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_gfx_draw_envs[1], red, green, blue);

    ClearImage(&clear_rect, 0, 0, 0);
    DrawSync(0);
    if (g_debug_font_print_enabled != 0) {
        debug_load_efc_font();
    }
    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
}

/* End a battle frame: wait for drawing and the vertical blank, flip the display
 * and draw environments and draw the ordering table. Returns VSync's result,
 * which the game loop keeps as the frame's hsync count.
 *
 * At vsync interval 1 the wait is one vertical blank. Otherwise it is
 * g_system_vsync_interval blanks, except in the effect and action-execute
 * states: there it is 2 blanks, or 3 while g_system_frame_pacing_timer runs (4 while it
 * is 16 or more), with the timer counting down each frame, and g_system_min_vsync_interval
 * replaces it when larger. The test for a g_system_min_vsync_interval of 1 cannot succeed,
 * since the value it is compared with is at least 2. */
s32 state_sync_frame(u32* ordering_table) {
    s32 wait;
    s32 sync_wait;
    s32 timer;
    s32 sync_result;

    if (g_system_vsync_interval == 1) {
        DrawSync(0);
        sync_result = VSync(0);
    } else {
        DrawSync(0);
        if (g_game_state == GAME_STATE_EFFECT || g_game_state == GAME_STATE_ACTION_EXECUTE) {
            timer = g_system_frame_pacing_timer;
            if (timer >= 0x10) {
                wait = 4;
                g_system_frame_pacing_timer = timer - 1;
            } else if (timer != 0) {
                wait = 3;
                g_system_frame_pacing_timer = timer - 1;
            } else {
                wait = 2;
            }
            sync_wait = g_system_min_vsync_interval;
            if (wait < sync_wait) {
                if (sync_wait == 1) {
                    sync_wait = 0;
                }
            } else {
                sync_wait = wait;
            }
            sync_result = VSync(sync_wait);
        } else {
            sync_result = VSync(g_system_vsync_interval);
        }
    }

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
    DrawOTag(ordering_table);
    FntFlush(-1);
    return sync_result;
}

/* End a frame of the deployment screen: wait for drawing and one vertical blank,
 * flip the display and draw environments and draw the ordering table. Returns
 * VSync's hsync count. */
s32 deploy_sync_and_submit_frame(u32* ordering_table) {
    s32 sync_result;

    DrawSync(0);
    sync_result = VSync(0);
    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
    DrawOTag(ordering_table);
    FntFlush(-1);
    return sync_result;
}
