#include "fft/battle.h"
#include "psx/types.h"

/* Tint a map palette bank and stage or apply the result.
 *
 * all_colors == 1 rewrites all 256 colors of the bank; otherwise only the
 * 16-color row color_index. Commands 0-3 start from the decoded component
 * bank, 4-8 from the packed mirror: 0/4/9 add the bias, 1/5 halve each
 * component first, 2/6 and 3/7 add the bias to a weighted gray
 * ((2R + 3G + B) / 6 or / 12), 8 copies the packed color, and 10 clears the
 * row tint flag and returns. Black components are left alone (their deltas
 * become neutral). A nonzero amount stores biased deltas and arms the row
 * animations with that step; zero applies the colors immediately and marks
 * the palette upload pending.
 *
 * The packed-color reads repeat `color_index * 16 + i` (no index local) and
 * the gray cases use case-local temporaries: both keep the target's loop
 * strength reduction and local register allocation. */
void battle_map_modify_palette(
    s32 command, s32 amount, s32 palette_index, s32 color_index, s32 all_colors, u16 red, u16 green, u16 blue) {
    s32 i;
    s16 out_red;
    s16 out_green;
    s16 out_blue;
    s32 red_part;
    map_palette_color_components_t* components;
    map_palette_color_components_t* row;
    map_palette_color_components_t* component;
    map_palette_animation_state_t* animation;

    if (all_colors == 1) {
        components = g_battle_map_palette_state.banks[palette_index].components;
        for (i = 0; i < 0x100; i++) {
            if (components[i].red_5bit + components[i].green_5bit + components[i].blue_5bit != 0) {
                switch (command) {
                case 0:
                    out_red = components[i].red_5bit + red;
                    out_green = components[i].green_5bit + green;
                    out_blue = components[i].blue_5bit + blue;
                    break;
                case 1:
                    out_red = (components[i].red_5bit >> 1) + red;
                    out_green = (components[i].green_5bit >> 1) + green;
                    out_blue = (components[i].blue_5bit >> 1) + blue;
                    break;
                case 2: {
                    s32 gray;
                    gray = (components[i].red_5bit * 2 + components[i].green_5bit * 3 + components[i].blue_5bit) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case 3: {
                    s32 gray;
                    gray = (components[i].red_5bit * 2 + components[i].green_5bit * 3 + components[i].blue_5bit) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case 4:
                case 9:
                    out_red = red + (g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x1f);
                    out_green
                        = green + ((g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x3e0) >> 5);
                    out_blue
                        = blue + ((g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x7c00) >> 10);
                    break;
                case 5:
                    red_part = g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green
                        = green + ((g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x3e0) >> 6);
                    out_blue
                        = blue + ((g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x7c00) >> 11);
                    break;
                case 6: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits = g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x3e0;
                    blue_bits = g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x7c00;
                    red_bits = g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x1f;
                    gray = (red_bits * 2 + (s32)(green_bits >> 5) * 3 + (s32)(blue_bits >> 10)) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case 7: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits = g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x3e0;
                    blue_bits = g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x7c00;
                    red_bits = g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x1f;
                    gray = (red_bits * 2 + (s32)(green_bits >> 5) * 3 + (s32)(blue_bits >> 10)) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case 8:
                    out_red = g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x1f;
                    out_green = (g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x3e0) >> 5;
                    out_blue = (g_battle_map_palette_state.banks[palette_index].packed_colors[i] & 0x7c00) >> 10;
                    break;
                case 10:
                    for (i = 15; i >= 0; i--) {
                        g_battle_map_palette_state.banks[palette_index].animations[i]._unknown_04[0] = 0;
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
                if ((s16)(out_red | out_green | out_blue) == 0) {
                    out_blue = 1;
                }
                if (amount != 0) {
                    components[i].red_delta_biased = out_red - components[i].red_5bit + 0x1f;
                    components[i].green_delta_biased = out_green - components[i].green_5bit + 0x1f;
                    components[i].blue_delta_biased = out_blue - components[i].blue_5bit + 0x1f;
                } else {
                    components[i].red_5bit = out_red;
                    components[i].green_5bit = out_green;
                    components[i].blue_5bit = out_blue;
                    g_battle_map_palette_modified_colors[palette_index][i]
                        = out_red + (out_green << 5) + (out_blue << 10) + (components[i].alpha_bit << 15);
                }
            } else {
                components[i].red_delta_biased = components[i].green_delta_biased = components[i].blue_delta_biased
                    = 0x1f;
            }
        }
        if (amount != 0) {
            for (i = 0; i < 16; i++) {
                animation = &g_battle_map_palette_state.banks[palette_index].animations[i];
                animation->active = 1;
                animation->blend_step = 0;
                animation->delay_counter = 0;
                animation->mode = amount;
                if (command == 9) {
                    animation->_unknown_04[0] = 1;
                    animation->_unknown_04[1] = red;
                    animation->_unknown_04[2] = green;
                    animation->_unknown_04[3] = blue;
                }
            }
            return;
        }
        for (i = 15; i >= 0; i--) {
            g_battle_map_palette_state.banks[palette_index].animations[i].active = 0;
        }
        g_battle_map_palette_state.upload_pending = 1;
    } else {
        row = &g_battle_map_palette_state.banks[palette_index].components[color_index * 16];
        for (i = 0; i < 16; i++) {
            component = &row[i];
            if (component->red_5bit + component->green_5bit + component->blue_5bit != 0) {
                switch (command) {
                case 0:
                    out_red = component->red_5bit + red;
                    out_green = component->green_5bit + green;
                    out_blue = component->blue_5bit + blue;
                    break;
                case 1:
                    out_red = (component->red_5bit >> 1) + red;
                    out_green = (component->green_5bit >> 1) + green;
                    out_blue = (component->blue_5bit >> 1) + blue;
                    break;
                case 2: {
                    s32 gray;
                    gray = (component->red_5bit * 2 + component->green_5bit * 3 + component->blue_5bit) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case 3: {
                    s32 gray;
                    gray = (component->red_5bit * 2 + component->green_5bit * 3 + component->blue_5bit) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case 4:
                case 9:
                    out_red = red
                        + (g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x1f);
                    out_green = green
                        + ((g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x3e0)
                            >> 5);
                    out_blue = blue
                        + ((g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i]
                               & 0x7c00)
                            >> 10);
                    break;
                case 5:
                    red_part
                        = g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green = green
                        + ((g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x3e0)
                            >> 6);
                    out_blue = blue
                        + ((g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i]
                               & 0x7c00)
                            >> 11);
                    break;
                case 6: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits
                        = g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x3e0;
                    blue_bits
                        = g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x7c00;
                    red_bits
                        = g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x1f;
                    gray = (red_bits * 2 + (s32)(green_bits >> 5) * 3 + (s32)(blue_bits >> 10)) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case 7: {
                    s32 red_bits;
                    u32 green_bits;
                    u32 blue_bits;
                    s32 gray;
                    green_bits
                        = g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x3e0;
                    blue_bits
                        = g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x7c00;
                    red_bits
                        = g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x1f;
                    gray = (red_bits * 2 + (s32)(green_bits >> 5) * 3 + (s32)(blue_bits >> 10)) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                }
                case 8:
                    out_red
                        = g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x1f;
                    out_green
                        = (g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x3e0)
                        >> 5;
                    out_blue
                        = (g_battle_map_palette_state.banks[palette_index].packed_colors[color_index * 16 + i] & 0x7c00)
                        >> 10;
                    break;
                case 10:
                    g_battle_map_palette_state.banks[palette_index].animations[color_index]._unknown_04[0] = 0;
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
                if ((s16)(out_red | out_green | out_blue) == 0) {
                    out_blue = 1;
                }
                if (amount != 0) {
                    component->red_delta_biased = out_red - component->red_5bit + 0x1f;
                    component->green_delta_biased = out_green - component->green_5bit + 0x1f;
                    component->blue_delta_biased = out_blue - component->blue_5bit + 0x1f;
                } else {
                    component->red_5bit = out_red;
                    component->green_5bit = out_green;
                    component->blue_5bit = out_blue;
                    g_battle_map_palette_modified_colors[palette_index][color_index * 16 + i]
                        = out_red + (out_green << 5) + (out_blue << 10) + (component->alpha_bit << 15);
                }
            } else {
                component->red_delta_biased = component->green_delta_biased = component->blue_delta_biased = 0x1f;
            }
        }
        if (amount != 0) {
            animation = &g_battle_map_palette_state.banks[palette_index].animations[color_index];
            animation->active = 1;
            animation->blend_step = 0;
            animation->delay_counter = 0;
            animation->mode = amount;
            if (command == 9) {
                animation->_unknown_04[0] = 1;
                animation->_unknown_04[1] = red;
                animation->_unknown_04[2] = green;
                animation->_unknown_04[3] = blue;
            }
            return;
        }
        g_battle_map_palette_state.banks[palette_index].animations[color_index].active = 0;
        g_battle_map_palette_state.upload_pending = 1;
    }
}
