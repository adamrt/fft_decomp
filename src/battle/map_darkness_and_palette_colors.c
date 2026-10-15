/* Map darkness and palette colors. Its start is a cut chosen by topic; its end is a cut chosen by topic. */

#include "fft/battle_map.h"
#include "psx/types.h"

/* Start a fade of the map darkness toward a color derived from mode, with
 * red/green/blue as 8-bit offsets applied in 16.16 fixed point.
 *
 * Modes 0..3 derive the base from the live transition channels, 4..9 from the
 * last applied color, and mode 10 only clears the repeat record. Modes 2, 3,
 * 6 and 7 replace the base with the grayscale (2R + 3G + B) / 6, halved again
 * for 3 and 7. Mode 9 additionally records the offsets so the map animation
 * updater can replay them.
 *
 * There is no default case: modes 11 and above use the uninitialized target
 * color, as the target does. */
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
