/* Map CLUT row writes, upload and the gradient/ambient/darkness seeds. */

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

/* Write one CLUT of a row from the colors at `source` with the red, green and
 * blue offsets added: battle_gfx_modify_vram_palette in MAP_COLOR_MODE_RESTORE, a
 * mode in which it neither halves nor grays the colors. */
void map_update_palette_from_other(
    const rgba5551_t* source, s32 clut_row, s32 clut_index, s32 preset, s16 red, s16 green, s16 blue, s32 final_value) {
    battle_gfx_modify_vram_palette(
        MAP_COLOR_MODE_RESTORE, source, clut_row, clut_index, preset, red, green, blue, final_value);
}

/* Load the two map background gradient colors for the current map. */
void map_init_background_gradient(void) {
    map_background_gradient_colors_t* colors;

    colors = map_light_state_command(MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT, colors);
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
void map_init_ambient_light(void) {
    CVECTOR* color;

    color = map_light_state_command(MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR, color);
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
void map_init_darkness(void) {
    CVECTOR* color;

    color = map_light_state_command(MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR, color);
    g_map_darkness_color = *color;
    g_map_darkness_transition.channels.red = color->r << 16;
    g_map_darkness_transition.channels.green = color->g << 16;
    g_map_darkness_transition.channels.blue = color->b << 16;
}

/* Upload the pending CLUT rows as one image to the lines below the rows, and
 * on the next call move them into place. */
void map_upload_clut_rows(void) {
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
void map_init_palette_state(void) {
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
    map_reset_clut_cycles();
}
