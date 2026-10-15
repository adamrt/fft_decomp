#include "fft/wldcore.h"
#include "psx/types.h"

/* Start a screen fade of `duration` frames in the direction of `flags`,
 * unless the screen is already faded that way. The flags can also start the
 * world-map music with a fade-in or fade the music out and unload it. */
void wldcore_fade_start_screen(s32 flags, s32 duration) {
    if ((g_wldcore_screen_fade_state.flags[0] & WLDCORE_SCREEN_FADE_OUT) != (flags & WLDCORE_SCREEN_FADE_OUT)) {
        g_wldcore_screen_fade_state.flags[0] = flags | WLDCORE_SCREEN_FADE_RUNNING;
        g_wldcore_screen_fade_state.elapsed = 0;
        g_wldcore_screen_fade_state.duration = duration;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_FADING;
        if (flags & WLDCORE_SCREEN_FADE_START_MUSIC) {
            wldcore_sound_enqueue_audio_command(
                WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
        }
        if (flags & WLDCORE_SCREEN_FADE_STOP_MUSIC) {
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 4);
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_UNLOAD, 2);
        }
    }
}

/* Steps the world screen fade: ramps the menu brightness and the first fill box
 * color toward the target until the elapsed counter reaches the duration. */
s32 wldcore_fade_step_screen(void) {
    u32 flags;
    s32 elapsed;
    s32 brightness;
    s32 box_level;

    flags = g_wldcore_screen_fade_state.flags[0];
    if (flags & WLDCORE_SCREEN_FADE_RUNNING) {
        if (g_wldcore_screen_fade_state.elapsed == g_wldcore_screen_fade_state.duration) {
            g_wldcore_screen_fade_state.flags[0] = flags
                & ~(WLDCORE_SCREEN_FADE_RUNNING | WLDCORE_SCREEN_FADE_REDRAW_MAP | WLDCORE_SCREEN_FADE_BOX_VISIBLE);
        } else {
            if (g_wldcore_screen_fade_state.elapsed + 1 == g_wldcore_screen_fade_state.duration) {
                g_main_system_flags &= ~MAIN_SYSTEM_FLAG_FADING;
                wldcore_sound_wait_for_queue_drain();
            }
            if (g_wldcore_screen_fade_state.flags[0] & WLDCORE_SCREEN_FADE_OUT) {
                elapsed = g_wldcore_screen_fade_state.elapsed;
                brightness = 0x70 - (elapsed << 7) / g_wldcore_screen_fade_state.duration;
                box_level = (elapsed << 8) / g_wldcore_screen_fade_state.duration + 0x20;
                if (brightness < 0) {
                    brightness = 0;
                }
                if (box_level >= 0x100) {
                    box_level = 0xFF;
                }
                if (elapsed < 0) {
                    brightness = 0x80;
                    box_level = 0;
                }
            } else {
                elapsed = g_wldcore_screen_fade_state.elapsed;
                brightness = (elapsed << 7) / g_wldcore_screen_fade_state.duration + 0x10;
                box_level = 0xC0 - (elapsed << 8) / g_wldcore_screen_fade_state.duration;
                if (brightness > 0x80) {
                    brightness = 0x80;
                }
                if (box_level < 0) {
                    box_level = 0;
                }
                if (elapsed < 0) {
                    brightness = 0;
                    box_level = 0x80;
                }
            }
            g_wldcore_screen_fade_state.elapsed++;
            world_menu_set_brightness(brightness, brightness, brightness);
            g_wldcore_screen_brightness_rgb.r = brightness;
            g_wldcore_screen_brightness_rgb.g = brightness;
            g_wldcore_screen_brightness_rgb.b = brightness;
            g_wldcore_screen_fade_state.boxes[0].r = box_level;
            g_wldcore_screen_fade_state.boxes[0].g = box_level;
            g_wldcore_screen_fade_state.boxes[0].b = box_level;
            if (g_wldcore_screen_fade_state.flags[0] & WLDCORE_SCREEN_FADE_REDRAW_MAP) {
                g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
            }
            return 1;
        }
    }
    return 0;
}

/* Copies a packed 24-bit RGB triple, applying the global per-channel tint
 * when bit 0 of the 32-bit fade state is set. Each channel is scaled by
 * ((c << 16) / 128) * factor >> 16, i.e. c * factor / 128, and clamped to
 * 255 with no lower clamp; with the factors at their 0x80 default the scale
 * is the identity. When the bit is clear the triple is copied verbatim,
 * which the 4-byte alignment-1 struct assignment emits as the unaligned
 * lwl/lwr + swl/swr quartet. */
void wldcore_gfx_copy_color_with_tint(CVECTOR* source, CVECTOR* out) {
    s32 red;
    s32 green;
    s32 blue;

    if (g_wldcore_screen_fade_state.flags[0] & WLDCORE_SCREEN_FADE_RUNNING) {
        red = ((source->r << 16) / 128 * g_wldcore_screen_brightness_rgb.r) >> 16;
        green = ((source->g << 16) / 128 * g_wldcore_screen_brightness_rgb.g) >> 16;
        blue = ((source->b << 16) / 128 * g_wldcore_screen_brightness_rgb.b) >> 16;
        if (red >= 256) {
            red = 255;
        }
        if (green >= 256) {
            green = 255;
        }
        if (blue >= 256) {
            blue = 255;
        }
        out->r = red;
        out->g = green;
        out->b = blue;
    } else {
        *out = *source;
    }
}

/* Sort the two screen-fade boxes into the active ordering table at their
 * priorities. A box is drawn while its fade runs or it is flagged visible,
 * and only when its color is not black. */
void wldcore_fade_draw_screen_overlays(void) {
    u32* flags;
    GsBOXF* boxes;
    s32* priorities;
    s32 i;

    flags = g_wldcore_screen_fade_state.flags;
    boxes = g_wldcore_screen_fade_state.boxes;
    for (i = 0; i < 2; i++) {
        /* Reloaded inside the loop so that the priority walk, not its invariant
         * copy, holds the register the exit test compares. */
        priorities = g_wldcore_screen_fade_state.priorities;
        if (flags[i] & (WLDCORE_SCREEN_FADE_RUNNING | WLDCORE_SCREEN_FADE_BOX_VISIBLE)) {
            if (g_wldcore_screen_fade_state.boxes[i].r != 0 || g_wldcore_screen_fade_state.boxes[i].g != 0
                || g_wldcore_screen_fade_state.boxes[i].b != 0) {
                GsSortBoxFill(&boxes[i], &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index], priorities[i]);
            }
        }
    }
}
