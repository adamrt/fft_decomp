#include "fft/battle.h"
#include "psx/types.h"

/* Advances the per-frame map animations: texture palette rotations, the
 * background gradient, ambient light and darkness transitions, and the
 * per-bank palette colour blends.
 *
 * red/green/blue are s16: the extensions of their u8 + s8 sums fold into
 * copies, which reproduces the target's separate stored and packed values
 * and the reloads of the component bytes. The components are indexed from a
 * row pointer so loop.c reduces one address giv, as in the target. */
void battle_map_update_animations(void) {
    map_palette_color_components_t saved_component;
    map_background_gradient_colors_t gradient;
    map_texture_animation_state_t* state;
    map_palette_runtime_bank_t* bank;
    map_palette_color_components_t* component;
    s32 i;
    s32 j;
    s32 row;
    u16 last;
    s16 red;
    s16 green;
    s16 blue;
    u16 saved_packed;
    u16 saved_modified;
    u8* repeat;
    map_color_transition_t* transition;
    s32 step;

    for (i = 0; i < 8; i++) {
        state = &g_battle_map_texture_animation_states[i];
        if (state->active != 0) {
            state->elapsed_frames++;
            if (state->elapsed_frames >= state->frame_duration) {
                state->elapsed_frames = 0;
                bank = &g_battle_map_palette_state.banks[state->polygon_group];
                saved_component = bank->components[state->last_polygon];
                last = state->last_polygon;
                saved_packed = bank->packed_colors[last];
                saved_modified = g_battle_map_palette_modified_colors[state->polygon_group][last];
                if (state->first_polygon < last) {
                    for (row = last; state->first_polygon < row; row--) {
                        bank->components[row] = bank->components[row - 1];
                        bank->packed_colors[row] = bank->packed_colors[row - 1];
                        g_battle_map_palette_modified_colors[state->polygon_group][row]
                            = g_battle_map_palette_modified_colors[state->polygon_group][row - 1];
                    }
                } else {
                    for (row = last; row < state->first_polygon; row++) {
                        bank->components[row] = bank->components[row + 1];
                        bank->packed_colors[row] = bank->packed_colors[row + 1];
                        g_battle_map_palette_modified_colors[state->polygon_group][row]
                            = g_battle_map_palette_modified_colors[state->polygon_group][row + 1];
                    }
                }
                bank->components[state->first_polygon] = saved_component;
                bank->packed_colors[state->first_polygon] = saved_packed;
                g_battle_map_palette_modified_colors[state->polygon_group][state->first_polygon] = saved_modified;
                g_battle_map_palette_state.upload_pending = 1;
            }
        }
    }

    if (g_map_background_gradient_transition.active != 0) {
        map_color_transition_channels_t* channels;
        map_gradient_transition_t* gradient_transition;

        channels = g_map_background_gradient_transition.channels;
        for (i = 0; i < 2; i++) {
            channels[i].red += channels[i].red_step;
            channels[i].green += channels[i].green_step;
            channels[i].blue += channels[i].blue_step;
        }
        gradient.first.red = channels[0].red / 0x10000;
        gradient.first.green = channels[0].green / 0x10000;
        gradient.first.blue = channels[0].blue / 0x10000;
        gradient.second.red = channels[1].red / 0x10000;
        gradient.second.green = channels[1].green / 0x10000;
        gradient.second.blue = channels[1].blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, (u8*)&gradient);
        repeat = &g_map_background_gradient_transition.period;
        /* Barrier: hides the constant so period and the byte after it are read through s0. */
        __asm__("" : "=r"(repeat) : "0"(repeat));
        gradient_transition = &g_map_background_gradient_transition;
        if (repeat[0] >= 4) {
            if (++gradient_transition->tick >= repeat[0] >> 2) {
                gradient_transition->tick = 0;
                if (++gradient_transition->phase >= 0x20) {
                    if (repeat[1] == 1) {
                        battle_map_blend_background_gradient_color(8, repeat[0], 0, 0, 0);
                        repeat[1] = 2;
                    } else if (repeat[1] == 2) {
                        battle_map_blend_background_gradient_color(9, repeat[0], gradient_transition->_unknown_04[1],
                            gradient_transition->_unknown_04[2], gradient_transition->_unknown_04[3]);
                        repeat[1] = 1;
                    } else {
                        goto deactivate;
                    }
                }
            }
        } else if (++gradient_transition->phase >= repeat[0] * 8) {
            if (repeat[1] == 1) {
                battle_map_blend_background_gradient_color(8, repeat[0], 0, 0, 0);
                repeat[1] = 2;
            } else if (repeat[1] == 2) {
                battle_map_blend_background_gradient_color(9, repeat[0], gradient_transition->_unknown_04[1],
                    gradient_transition->_unknown_04[2], gradient_transition->_unknown_04[3]);
                repeat[1] = 1;
            } else {
            /* Shared with the first arm so this store starts a fresh CSE
             * block and stays absolute instead of reusing
             * gradient_transition. */
            deactivate:
                g_map_background_gradient_transition.active = 0;
            }
        }
    }

    transition = &g_battle_map_ambient_light_transition;
    /* Barrier: hides the constant so active and the red channel are accessed through s0. */
    __asm__("" : "=r"(transition) : "0"(transition));
    if (transition->active != 0) {
        s32 red_level;

        red_level = transition->channels.red += g_battle_map_ambient_light_transition.channels.red_step;
        g_battle_map_ambient_light_transition.channels.green
            += g_battle_map_ambient_light_transition.channels.green_step;
        g_battle_map_ambient_light_transition.channels.blue += g_battle_map_ambient_light_transition.channels.blue_step;
        gradient.first.red = red_level / 0x10000;
        gradient.first.green = g_battle_map_ambient_light_transition.channels.green / 0x10000;
        gradient.first.blue = g_battle_map_ambient_light_transition.channels.blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR, (u8*)&gradient);
        if (g_battle_map_ambient_light_transition.period >= 4) {
            if (++g_battle_map_ambient_light_transition.tick >= g_battle_map_ambient_light_transition.period >> 2) {
                g_battle_map_ambient_light_transition.tick = 0;
                if (++g_battle_map_ambient_light_transition.phase >= 0x20) {
                    if (g_battle_map_ambient_light_transition._unknown_04[0] == 1) {
                        battle_map_blend_ambient_light_color(8, g_battle_map_ambient_light_transition.period, 0, 0, 0);
                        g_battle_map_ambient_light_transition._unknown_04[0] = 2;
                    } else if (g_battle_map_ambient_light_transition._unknown_04[0] == 2) {
                        battle_map_blend_ambient_light_color(9, g_battle_map_ambient_light_transition.period,
                            g_battle_map_ambient_light_transition._unknown_04[1],
                            g_battle_map_ambient_light_transition._unknown_04[2],
                            g_battle_map_ambient_light_transition._unknown_04[3]);
                        g_battle_map_ambient_light_transition._unknown_04[0] = 1;
                    } else {
                        transition->active = 0;
                    }
                }
            }
        } else if (++g_battle_map_ambient_light_transition.phase >= g_battle_map_ambient_light_transition.period * 8) {
            if (g_battle_map_ambient_light_transition._unknown_04[0] == 1) {
                battle_map_blend_ambient_light_color(8, g_battle_map_ambient_light_transition.period, 0, 0, 0);
                g_battle_map_ambient_light_transition._unknown_04[0] = 2;
            } else if (g_battle_map_ambient_light_transition._unknown_04[0] == 2) {
                battle_map_blend_ambient_light_color(9, g_battle_map_ambient_light_transition.period,
                    g_battle_map_ambient_light_transition._unknown_04[1],
                    g_battle_map_ambient_light_transition._unknown_04[2],
                    g_battle_map_ambient_light_transition._unknown_04[3]);
                g_battle_map_ambient_light_transition._unknown_04[0] = 1;
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
        gradient.first.red = red_level / 0x10000;
        gradient.first.green = g_battle_map_darkness_transition.channels.green / 0x10000;
        gradient.first.blue = g_battle_map_darkness_transition.channels.blue / 0x10000;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, (u8*)&gradient);
        if (g_battle_map_darkness_transition.period >= 4) {
            if (++g_battle_map_darkness_transition.tick >= g_battle_map_darkness_transition.period >> 2) {
                g_battle_map_darkness_transition.tick = 0;
                if (++g_battle_map_darkness_transition.phase >= 0x20) {
                    if (g_battle_map_darkness_transition._unknown_04[0] == 1) {
                        battle_map_blend_darkness_color(8, g_battle_map_darkness_transition.period, 0, 0, 0);
                        g_battle_map_darkness_transition._unknown_04[0] = 2;
                    } else if (g_battle_map_darkness_transition._unknown_04[0] == 2) {
                        battle_map_blend_darkness_color(9, g_battle_map_darkness_transition.period,
                            g_battle_map_darkness_transition._unknown_04[1],
                            g_battle_map_darkness_transition._unknown_04[2],
                            g_battle_map_darkness_transition._unknown_04[3]);
                        g_battle_map_darkness_transition._unknown_04[0] = 1;
                    } else {
                        transition->active = 0;
                    }
                }
            }
        } else if (++g_battle_map_darkness_transition.phase >= g_battle_map_darkness_transition.period * 8) {
            if (g_battle_map_darkness_transition._unknown_04[0] == 1) {
                battle_map_blend_darkness_color(8, g_battle_map_darkness_transition.period, 0, 0, 0);
                g_battle_map_darkness_transition._unknown_04[0] = 2;
            } else if (g_battle_map_darkness_transition._unknown_04[0] == 2) {
                battle_map_blend_darkness_color(9, g_battle_map_darkness_transition.period,
                    g_battle_map_darkness_transition._unknown_04[1], g_battle_map_darkness_transition._unknown_04[2],
                    g_battle_map_darkness_transition._unknown_04[3]);
                g_battle_map_darkness_transition._unknown_04[0] = 1;
            } else {
                transition->active = 0;
            }
        }
        if (g_battle_map_weather_flags & 2) {
            red = g_battle_map_darkness_transition.channels.red / 0x80000;
            green = g_battle_map_darkness_transition.channels.green / 0x80000;
            blue = g_battle_map_darkness_transition.channels.blue / 0x80000;
            g_battle_map_palette_modified_colors[0][0] = red + (green << 5) + (blue << 10) + 0x8000;

            g_battle_map_palette_state.upload_pending = 1;
        }
    }

    for (i = 0; i < 14; i++) {
        if (g_battle_map_palette_state.banks[i].enabled == 0) {
            for (row = 0; row < 16; row++) {
                if (g_battle_map_palette_state.banks[i].animations[row].active == 0) {
                    continue;
                }
                if (g_battle_map_palette_state.banks[i].animations[row].mode >= 4) {
                    if (++g_battle_map_palette_state.banks[i].animations[row].delay_counter
                        >= g_battle_map_palette_state.banks[i].animations[row].mode >> 2) {
                        step = g_battle_map_palette_state.banks[i].animations[row].blend_step;
                        g_battle_map_palette_state.banks[i].animations[row].delay_counter = 0;
                        component = &g_battle_map_palette_state.banks[i].components[row * 16];
                        for (j = 0; j < 16; j++) {
                            if (component[j].red_5bit + component[j].green_5bit + component[j].blue_5bit != 0) {
                                red = component[j].red_5bit
                                    + (s8)g_battle_map_palette_blend_steps_32[component[j].red_delta_biased][step];
                                green = component[j].green_5bit
                                    + (s8)g_battle_map_palette_blend_steps_32[component[j].green_delta_biased][step];
                                blue = component[j].blue_5bit
                                    + (s8)g_battle_map_palette_blend_steps_32[component[j].blue_delta_biased][step];
                                if ((red | green | blue) == 0) {
                                    blue = 1;
                                }
                                component[j].blue_5bit = blue;
                                component[j].red_5bit = red;
                                component[j].green_5bit = green;
                                g_battle_map_palette_modified_colors[i][row * 16 + j]
                                    = red + (green << 5) + (blue << 10) + (component[j].alpha_bit << 15);
                            }
                        }

                        if (++g_battle_map_palette_state.banks[i].animations[row].blend_step >= 0x20) {
                            if (g_battle_map_palette_state.banks[i].animations[row]._unknown_04[0] == 1) {
                                battle_map_modify_palette(
                                    8, g_battle_map_palette_state.banks[i].animations[row].mode, i, row, 0, 0, 0, 0);
                                g_battle_map_palette_state.banks[i].animations[row]._unknown_04[0] = 2;
                            } else if (g_battle_map_palette_state.banks[i].animations[row]._unknown_04[0] == 2) {
                                battle_map_modify_palette(9, g_battle_map_palette_state.banks[i].animations[row].mode,
                                    i, row, 0, g_battle_map_palette_state.banks[i].animations[row]._unknown_04[1],
                                    g_battle_map_palette_state.banks[i].animations[row]._unknown_04[2],
                                    g_battle_map_palette_state.banks[i].animations[row]._unknown_04[3]);
                                g_battle_map_palette_state.banks[i].animations[row]._unknown_04[0] = 1;
                            } else {
                                g_battle_map_palette_state.banks[i].animations[row].active = 0;
                            }
                        }
                    }
                } else {
                    step = g_battle_map_palette_state.banks[i].animations[row].blend_step;
                    component = &g_battle_map_palette_state.banks[i].components[row * 16];
                    for (j = 0; j < 16; j++) {
                        if (component[j].red_5bit + component[j].green_5bit + component[j].blue_5bit != 0) {
                            red = component[j].red_5bit
                                + (s8)g_battle_map_palette_blend_steps_8[component[j].red_delta_biased][step];
                            green = component[j].green_5bit
                                + (s8)g_battle_map_palette_blend_steps_8[component[j].green_delta_biased][step];
                            blue = component[j].blue_5bit
                                + (s8)g_battle_map_palette_blend_steps_8[component[j].blue_delta_biased][step];
                            if ((red | green | blue) == 0) {
                                blue = 1;
                            }
                            component[j].blue_5bit = blue;
                            component[j].red_5bit = red;
                            component[j].green_5bit = green;
                            g_battle_map_palette_modified_colors[i][row * 16 + j]
                                = red + (green << 5) + (blue << 10) + (component[j].alpha_bit << 15);
                        }
                    }

                    if (++g_battle_map_palette_state.banks[i].animations[row].blend_step >= 8) {
                        if (g_battle_map_palette_state.banks[i].animations[row]._unknown_04[0] == 1) {
                            battle_map_modify_palette(
                                8, g_battle_map_palette_state.banks[i].animations[row].mode, i, row, 0, 0, 0, 0);
                            g_battle_map_palette_state.banks[i].animations[row]._unknown_04[0] = 2;
                        } else if (g_battle_map_palette_state.banks[i].animations[row]._unknown_04[0] == 2) {
                            battle_map_modify_palette(9, g_battle_map_palette_state.banks[i].animations[row].mode, i,
                                row, 0, g_battle_map_palette_state.banks[i].animations[row]._unknown_04[1],
                                g_battle_map_palette_state.banks[i].animations[row]._unknown_04[2],
                                g_battle_map_palette_state.banks[i].animations[row]._unknown_04[3]);
                            g_battle_map_palette_state.banks[i].animations[row]._unknown_04[0] = 1;
                        } else {
                            g_battle_map_palette_state.banks[i].animations[row].active = 0;
                        }
                    }
                }
                g_battle_map_palette_state.upload_pending = 1;
            }
        } else if (g_battle_map_palette_state.banks[i].animations[0].active != 0) {
            if (g_battle_map_palette_state.banks[i].animations[0].mode >= 4) {
                if (++g_battle_map_palette_state.banks[i].animations[0].delay_counter
                    >= g_battle_map_palette_state.banks[i].animations[0].mode >> 2) {
                    step = g_battle_map_palette_state.banks[i].animations[0].blend_step;
                    g_battle_map_palette_state.banks[i].animations[0].delay_counter = 0;
                    component = g_battle_map_palette_state.banks[i].components;
                    for (j = 0; j < 256; j++) {
                        if (component[j].red_5bit + component[j].green_5bit + component[j].blue_5bit != 0) {
                            red = component[j].red_5bit
                                + (s8)g_battle_map_palette_blend_steps_32[component[j].red_delta_biased][step];
                            green = component[j].green_5bit
                                + (s8)g_battle_map_palette_blend_steps_32[component[j].green_delta_biased][step];
                            blue = component[j].blue_5bit
                                + (s8)g_battle_map_palette_blend_steps_32[component[j].blue_delta_biased][step];
                            if ((red | green | blue) == 0) {
                                blue = 1;
                            }
                            component[j].red_5bit = red;
                            component[j].green_5bit = green;
                            component[j].blue_5bit = blue;
                            g_battle_map_palette_modified_colors[i][j]
                                = red + (green << 5) + (blue << 10) + (component[j].alpha_bit << 15);
                        }
                    }

                    if (++g_battle_map_palette_state.banks[i].animations[0].blend_step >= 0x20) {
                        if (g_battle_map_palette_state.banks[i].animations[0]._unknown_04[0] == 1) {
                            battle_map_modify_palette(
                                8, g_battle_map_palette_state.banks[i].animations[0].mode, i, 0, 1, 0, 0, 0);
                            g_battle_map_palette_state.banks[i].animations[0]._unknown_04[0] = 2;
                        } else if (g_battle_map_palette_state.banks[i].animations[0]._unknown_04[0] == 2) {
                            battle_map_modify_palette(9, g_battle_map_palette_state.banks[i].animations[0].mode, i, 0,
                                1, g_battle_map_palette_state.banks[i].animations[0]._unknown_04[1],
                                g_battle_map_palette_state.banks[i].animations[0]._unknown_04[2],
                                g_battle_map_palette_state.banks[i].animations[0]._unknown_04[3]);
                            g_battle_map_palette_state.banks[i].animations[0]._unknown_04[0] = 1;
                        } else {
                            for (j = 15; j >= 0; j--) {
                                g_battle_map_palette_state.banks[i].animations[j].active = 0;
                            }
                        }
                    }
                }
            } else {
                step = g_battle_map_palette_state.banks[i].animations[0].blend_step;
                component = g_battle_map_palette_state.banks[i].components;
                for (j = 0; j < 256; j++) {
                    if (component[j].red_5bit + component[j].green_5bit + component[j].blue_5bit != 0) {
                        red = component[j].red_5bit
                            + (s8)g_battle_map_palette_blend_steps_8[component[j].red_delta_biased][step];
                        green = component[j].green_5bit
                            + (s8)g_battle_map_palette_blend_steps_8[component[j].green_delta_biased][step];
                        blue = component[j].blue_5bit
                            + (s8)g_battle_map_palette_blend_steps_8[component[j].blue_delta_biased][step];
                        if ((red | green | blue) == 0) {
                            blue = 1;
                        }
                        component[j].red_5bit = red;
                        component[j].green_5bit = green;
                        component[j].blue_5bit = blue;
                        g_battle_map_palette_modified_colors[i][j]
                            = red + (green << 5) + (blue << 10) + (component[j].alpha_bit << 15);
                    }
                }

                if (++g_battle_map_palette_state.banks[i].animations[0].blend_step >= 8) {
                    if (g_battle_map_palette_state.banks[i].animations[0]._unknown_04[0] == 1) {
                        battle_map_modify_palette(
                            8, g_battle_map_palette_state.banks[i].animations[0].mode, i, 0, 1, 0, 0, 0);
                        g_battle_map_palette_state.banks[i].animations[0]._unknown_04[0] = 2;
                    } else if (g_battle_map_palette_state.banks[i].animations[0]._unknown_04[0] == 2) {
                        battle_map_modify_palette(9, g_battle_map_palette_state.banks[i].animations[0].mode, i, 0, 1,
                            g_battle_map_palette_state.banks[i].animations[0]._unknown_04[1],
                            g_battle_map_palette_state.banks[i].animations[0]._unknown_04[2],
                            g_battle_map_palette_state.banks[i].animations[0]._unknown_04[3]);
                        g_battle_map_palette_state.banks[i].animations[0]._unknown_04[0] = 1;
                    } else {
                        for (j = 15; j >= 0; j--) {
                            g_battle_map_palette_state.banks[i].animations[j].active = 0;
                        }
                    }
                }
            }
            if (i == 0 && (g_battle_map_weather_flags & 2)) {
                red = g_battle_map_darkness_transition.channels.red / 0x80000;
                green = g_battle_map_darkness_transition.channels.green / 0x80000;
                blue = g_battle_map_darkness_transition.channels.blue / 0x80000;
                g_battle_map_palette_modified_colors[0][0] = red + (green << 5) + (blue << 10) + 0x8000;
            }
            g_battle_map_palette_state.upload_pending = 1;
        }
    }
}
