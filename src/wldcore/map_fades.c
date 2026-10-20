/* World-map fades hide render queues, tint colors and cover the display while menus change. */

#include "fft/gfx.h"
#include "fft/music.h"
#include "fft/screen.h"
#include "fft/system.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Clear (nonzero argument) or set the blank flag on both display-object queues.
 * Both record types start with the same signed 32-bit flags field. */
void window_set_render_objects_visible(s32 clear) {
    s32 i;

    if (clear) {
        for (i = 0; i < g_window_aux_render_object_count; i++) {
            *(s32*)g_window_aux_render_object_queue[i] &= ~DISPLAY_OBJECT_BLANK;
        }
        for (i = 0; i < g_window_render_object_count; i++) {
            *(s32*)g_window_render_object_queue[i] &= ~DISPLAY_OBJECT_BLANK;
        }
    } else {
        for (i = 0; i < g_window_aux_render_object_count; i++) {
            *(s32*)g_window_aux_render_object_queue[i] |= DISPLAY_OBJECT_BLANK;
        }
        for (i = 0; i < g_window_render_object_count; i++) {
            *(s32*)g_window_render_object_queue[i] |= DISPLAY_OBJECT_BLANK;
        }
    }
}

/* Start a screen fade of `duration` frames unless the current fade already has the same direction
 * (WORLD_MAP_FADE_OUT bit); START_MUSIC also queues the world map theme with a fade-in,
 * STOP_MUSIC fades the music out and unloads it. */
void world_map_fade_start(s32 flags, s32 duration) {
    if ((g_world_map_fade.flags[0] & WORLD_MAP_FADE_OUT) != (flags & WORLD_MAP_FADE_OUT)) {
        g_world_map_fade.flags[0] = flags | WORLD_MAP_FADE_RUNNING;
        g_world_map_fade.elapsed = 0;
        g_world_map_fade.duration = duration;
        g_system_flags |= SYSTEM_FLAG_FADING;
        if (flags & WORLD_MAP_FADE_START_MUSIC) {
            music_queue_add(MUSIC_QUEUE_COMMAND_PLAY_TRACK, MUSIC_QUEUE_TRACK(1, MUSIC_WORLD_MAP));
            music_queue_add(MUSIC_QUEUE_COMMAND_FADE_IN, 0x10);
        }
        if (flags & WORLD_MAP_FADE_STOP_MUSIC) {
            music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 4);
            music_queue_add(MUSIC_QUEUE_COMMAND_UNLOAD, 2);
        }
    }
}

/* Steps the world screen fade: ramps the menu brightness and the first fill box
 * color toward the target until the elapsed counter reaches the duration. */
s32 world_map_fade_update(void) {
    u32 flags;
    s32 elapsed;
    s32 brightness;
    s32 box_level;

    flags = g_world_map_fade.flags[0];
    if (flags & WORLD_MAP_FADE_RUNNING) {
        if (g_world_map_fade.elapsed == g_world_map_fade.duration) {
            g_world_map_fade.flags[0]
                = flags & ~(WORLD_MAP_FADE_RUNNING | WORLD_MAP_FADE_REDRAW_MAP | WORLD_MAP_FADE_BOX_VISIBLE);
        } else {
            if (g_world_map_fade.elapsed + 1 == g_world_map_fade.duration) {
                g_system_flags &= ~SYSTEM_FLAG_FADING;
                music_queue_wait_until_empty();
            }
            if (g_world_map_fade.flags[0] & WORLD_MAP_FADE_OUT) {
                elapsed = g_world_map_fade.elapsed;
                brightness = 0x70 - (elapsed << 7) / g_world_map_fade.duration;
                box_level = (elapsed << 8) / g_world_map_fade.duration + 0x20;
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
                elapsed = g_world_map_fade.elapsed;
                brightness = (elapsed << 7) / g_world_map_fade.duration + 0x10;
                box_level = 0xC0 - (elapsed << 8) / g_world_map_fade.duration;
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
            g_world_map_fade.elapsed++;
            screen_brightness_set(brightness, brightness, brightness);
            g_world_map_brightness_rgb.r = brightness;
            g_world_map_brightness_rgb.g = brightness;
            g_world_map_brightness_rgb.b = brightness;
            g_world_map_fade.boxes[0].r = box_level;
            g_world_map_fade.boxes[0].g = box_level;
            g_world_map_fade.boxes[0].b = box_level;
            if (g_world_map_fade.flags[0] & WORLD_MAP_FADE_REDRAW_MAP) {
                g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
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
void world_map_fade_tint_color(CVECTOR* source, CVECTOR* out) {
    s32 red;
    s32 green;
    s32 blue;

    if (g_world_map_fade.flags[0] & WORLD_MAP_FADE_RUNNING) {
        red = ((source->r << 16) / 128 * g_world_map_brightness_rgb.r) >> 16;
        green = ((source->g << 16) / 128 * g_world_map_brightness_rgb.g) >> 16;
        blue = ((source->b << 16) / 128 * g_world_map_brightness_rgb.b) >> 16;
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
void world_map_fade_boxes_draw(void) {
    u32* flags;
    GsBOXF* boxes;
    s32* priorities;
    s32 i;

    flags = g_world_map_fade.flags;
    boxes = g_world_map_fade.boxes;
    for (i = 0; i < 2; i++) {
        /* Reloaded inside the loop so that the priority walk, not its invariant
         * copy, holds the register the exit test compares. */
        priorities = g_world_map_fade.priorities;
        if (flags[i] & (WORLD_MAP_FADE_RUNNING | WORLD_MAP_FADE_BOX_VISIBLE)) {
            if (g_world_map_fade.boxes[i].r != 0 || g_world_map_fade.boxes[i].g != 0
                || g_world_map_fade.boxes[i].b != 0) {
                GsSortBoxFill(&boxes[i], &g_world_map_ordering_tables[g_gfx_active_buffer_index], priorities[i]);
            }
        }
    }
}
