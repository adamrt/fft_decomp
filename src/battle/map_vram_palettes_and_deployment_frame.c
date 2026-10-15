/* Map vram palettes and deployment frame. Its start is a cut chosen by topic; its end is a boundary the binary proves.
 */

#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/pad.h"
#include "psx/types.h"

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

void map_freeze(void) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}

/* Resume the map texture animations paused by map_freeze. */
void map_unfreeze(void) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}

/* Event ColorField: tint or fade all 256 colors of the map CLUT row with
 * map_modify_palette. `first` is its color mode (map_color_mode_e) and
 * `second` its amount: 0 applies the colors at once, another value fades to
 * them. */
void map_color_field(s32 first, s32 second, s16 red, s16 green, s16 blue) {
    map_modify_palette(first, second, MAP_CLUT_ROW_MAP, 0, 1, red, green, blue);
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
    tile = map_get_tile_data_pointer(
        screen_coordinates.vx / MAP_TILE_SIZE, screen_coordinates.vz / MAP_TILE_SIZE, unit->map_z);

    map_modify_palette(preset_color, time, MAP_CLUT_ROW_UNITS, runtime_id, 0, red, green, blue);

    if (tile->depth_half_height.depth != 0
        && unit->world_coords.vy >= -(tile->height + tile->depth_half_height.depth) * 12) {
        map_modify_palette(preset_color, time, MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0, red, green, blue);
    } else {
        map_modify_palette(preset_color, time, MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0, -red, -green, -blue);
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
