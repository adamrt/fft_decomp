#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Apply a color modulation to the colors at `source` and store them into a map CLUT row.
 *
 * Preset 1 rewrites all 256 colors of the row; otherwise only the 16
 * colors of CLUT `clut_index` are rewritten. Mode 5 halves each component
 * before adding the bias, modes 6 and 7 add the bias to a weighted gray
 * ((2R + 3G + B) / 6 or / 12), and every other mode adds the bias directly.
 * Transparent (zero) colors are copied unchanged. Components are clamped to
 * 0-31, and a non-transparent color that clamps to black gets blue 1.
 *
 * `case 4` shares the default body: the extra case node makes GCC's switch
 * tree test mode 5 first, as the target does. */
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
