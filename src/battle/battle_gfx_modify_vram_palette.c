#include "fft/battle.h"
#include "psx/types.h"

/* Apply a color modulation to a CLUT and store it into a map palette bank.
 *
 * Preset 1 rewrites all 256 colors of the section; otherwise only the 16
 * colors of row unit_misc_id are rewritten. Mode 5 halves each component
 * before adding the bias, modes 6 and 7 add the bias to a weighted gray
 * ((2R + 3G + B) / 6 or / 12), and every other mode adds the bias directly.
 * Transparent (zero) colors are copied unchanged. Components are clamped to
 * 0-31, and a non-transparent color that clamps to black gets blue 1.
 *
 * `case 4` shares the default body: the extra case node makes GCC's switch
 * tree test mode 5 first, as the target does. */
void battle_gfx_modify_vram_palette(s32 mode, const u16* source, s32 section_id, s32 unit_misc_id, s32 preset, s16 red,
    s16 green, s16 blue, s32 final_value) {
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
    u32 alpha;
    map_palette_color_components_t* component;

    if (preset == 1) {
        i = 0;
        do {
            color = *source;
            if (*(const s16*)source != 0) {
                switch (mode) {
                case 5:
                    red_part = color & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green = green + ((color & 0x3e0) >> 6);
                    out_blue = blue + ((color & 0x7c00) >> 11);
                    break;
                case 6:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case 7:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case 4:
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
            alpha = color >> 15;
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
            if ((s16)(out_red | out_green | out_blue) == 0 && (color & 0x7fff)) {
                out_blue = 1;
            }
            if (final_value != 0) {
                g_battle_map_palette_modified_colors[section_id][i]
                    = out_red + (out_green << 5) + (out_blue << 10) + (alpha << 15);
                if (g_battle_map_palette_state.banks[section_id].animations[i / 16].active == 0) {
                    component = &g_battle_map_palette_state.banks[section_id].components[i];
                    component->red_5bit = out_red;
                    component->green_5bit = out_green;
                    component->blue_5bit = out_blue;
                    component->alpha_bit = alpha;
                }
            }
            g_battle_map_palette_state.banks[section_id].packed_colors[i]
                = out_red + (out_green << 5) + (out_blue << 10) + (alpha << 15);
            i++;
            source++;
        } while (i < 0x100);
    } else {
        i = 0;
        do {
            color = *source;
            if (*(const s16*)source != 0) {
                switch (mode) {
                case 5:
                    red_part = color & 0x1f;
                    out_red = red + (red_part >> 1);
                    out_green = green + ((color & 0x3e0) >> 6);
                    out_blue = blue + ((color & 0x7c00) >> 11);
                    break;
                case 6:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 6;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case 7:
                    gray_red = color & 0x1f;
                    gray_green = (color & 0x3e0) >> 5;
                    gray_blue = (color & 0x7c00) >> 10;
                    gray = (gray_red * 2 + gray_green * 3 + gray_blue) / 12;
                    out_red = red + gray;
                    out_green = green + gray;
                    out_blue = blue + gray;
                    break;
                case 4:
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
            alpha = color >> 15;
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
            if ((s16)(out_red | out_green | out_blue) == 0 && (color & 0x7fff)) {
                out_blue = 1;
            }
            if (final_value != 0) {
                index = unit_misc_id * 16 + i;
                g_battle_map_palette_modified_colors[section_id][index]
                    = out_red + (out_green << 5) + (out_blue << 10) + (alpha << 15);
                if (g_battle_map_palette_state.banks[section_id].animations[unit_misc_id].active == 0) {
                    component = &g_battle_map_palette_state.banks[section_id].components[index];
                    component->red_5bit = out_red;
                    component->green_5bit = out_green;
                    component->blue_5bit = out_blue;
                    component->alpha_bit = alpha;
                }
            }
            g_battle_map_palette_state.banks[section_id].packed_colors[unit_misc_id * 16 + i]
                = out_red + (out_green << 5) + (out_blue << 10) + (alpha << 15);
            i++;
            source++;
        } while (i < 16);
    }
    g_battle_map_palette_state.upload_pending = 1;
}
