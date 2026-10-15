#include "fft/battle_map.h"
#include "psx/types.h"

/* Advances the per-frame map animations: texture palette rotations, the
 * background gradient, ambient light and darkness transitions, and the
 * color blends of each map CLUT row.
 *
 * red/green/blue are s16: the extensions of their u8 + s8 sums fold into
 * copies, which reproduces the target's separate stored and packed values
 * and the reloads of the component bytes. The components are indexed from a
 * pointer to the CLUT's first color so loop.c reduces one address giv, as in the target. */
void map_update_animations(void) {
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
        map_light_state_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &gradient);
        if (g_map_background_gradient_transition.period >= 4) {
            if (++g_map_background_gradient_transition.tick >= g_map_background_gradient_transition.period >> 2) {
                g_map_background_gradient_transition.tick = 0;
                if (++g_map_background_gradient_transition.phase >= 0x20) {
                    if (g_map_background_gradient_transition.repeat == MAP_COLOR_REPEAT_TINTED) {
                        map_blend_background_gradient_color(
                            MAP_COLOR_MODE_RESTORE, g_map_background_gradient_transition.period, 0, 0, 0);
                        g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_map_background_gradient_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                        map_blend_background_gradient_color(MAP_COLOR_MODE_BASE_REPEAT,
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
                map_blend_background_gradient_color(
                    MAP_COLOR_MODE_RESTORE, g_map_background_gradient_transition.period, 0, 0, 0);
                g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
            } else if (g_map_background_gradient_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                map_blend_background_gradient_color(MAP_COLOR_MODE_BASE_REPEAT,
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
        map_light_state_command(MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, &gradient);
        if (g_map_ambient_light_transition.period >= 4) {
            if (++g_map_ambient_light_transition.tick >= g_map_ambient_light_transition.period >> 2) {
                g_map_ambient_light_transition.tick = 0;
                if (++g_map_ambient_light_transition.phase >= 0x20) {
                    if (g_map_ambient_light_transition.repeat == MAP_COLOR_REPEAT_TINTED) {
                        map_blend_ambient_light_color(
                            MAP_COLOR_MODE_RESTORE, g_map_ambient_light_transition.period, 0, 0, 0);
                        g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_map_ambient_light_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                        map_blend_ambient_light_color(MAP_COLOR_MODE_BASE_REPEAT, g_map_ambient_light_transition.period,
                            g_map_ambient_light_transition.repeat_color.r,
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
                map_blend_ambient_light_color(MAP_COLOR_MODE_RESTORE, g_map_ambient_light_transition.period, 0, 0, 0);
                g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
            } else if (g_map_ambient_light_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                map_blend_ambient_light_color(MAP_COLOR_MODE_BASE_REPEAT, g_map_ambient_light_transition.period,
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
        map_light_state_command(MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &gradient);
        if (g_map_darkness_transition.period >= 4) {
            if (++g_map_darkness_transition.tick >= g_map_darkness_transition.period >> 2) {
                g_map_darkness_transition.tick = 0;
                if (++g_map_darkness_transition.phase >= 0x20) {
                    if (g_map_darkness_transition.repeat == MAP_COLOR_REPEAT_TINTED) {
                        map_blend_darkness_color(MAP_COLOR_MODE_RESTORE, g_map_darkness_transition.period, 0, 0, 0);
                        g_map_darkness_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_map_darkness_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                        map_blend_darkness_color(MAP_COLOR_MODE_BASE_REPEAT, g_map_darkness_transition.period,
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
                map_blend_darkness_color(MAP_COLOR_MODE_RESTORE, g_map_darkness_transition.period, 0, 0, 0);
                g_map_darkness_transition.repeat = MAP_COLOR_REPEAT_RESTORED;
            } else if (g_map_darkness_transition.repeat == MAP_COLOR_REPEAT_RESTORED) {
                map_blend_darkness_color(MAP_COLOR_MODE_BASE_REPEAT, g_map_darkness_transition.period,
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
                                map_modify_palette(MAP_COLOR_MODE_RESTORE,
                                    g_map_palette_state.clut_rows[i].animations[k].mode, i, k, 0, 0, 0, 0);
                                g_map_palette_state.clut_rows[i].animations[k].repeat = MAP_COLOR_REPEAT_RESTORED;
                            } else if (g_map_palette_state.clut_rows[i].animations[k].repeat
                                == MAP_COLOR_REPEAT_RESTORED) {
                                map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT,
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
                            map_modify_palette(MAP_COLOR_MODE_RESTORE,
                                g_map_palette_state.clut_rows[i].animations[k].mode, i, k, 0, 0, 0, 0);
                            g_map_palette_state.clut_rows[i].animations[k].repeat = MAP_COLOR_REPEAT_RESTORED;
                        } else if (g_map_palette_state.clut_rows[i].animations[k].repeat == MAP_COLOR_REPEAT_RESTORED) {
                            map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT,
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
                            map_modify_palette(MAP_COLOR_MODE_RESTORE,
                                g_map_palette_state.clut_rows[i].animations[0].mode, i, 0, 1, 0, 0, 0);
                            g_map_palette_state.clut_rows[i].animations[0].repeat = MAP_COLOR_REPEAT_RESTORED;
                        } else if (g_map_palette_state.clut_rows[i].animations[0].repeat == MAP_COLOR_REPEAT_RESTORED) {
                            map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT,
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
                        map_modify_palette(MAP_COLOR_MODE_RESTORE, g_map_palette_state.clut_rows[i].animations[0].mode,
                            i, 0, 1, 0, 0, 0);
                        g_map_palette_state.clut_rows[i].animations[0].repeat = MAP_COLOR_REPEAT_RESTORED;
                    } else if (g_map_palette_state.clut_rows[i].animations[0].repeat == MAP_COLOR_REPEAT_RESTORED) {
                        map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT,
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
