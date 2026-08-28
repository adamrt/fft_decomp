#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Pending shared-header change: wldcore_sortbox_state_t.unknown_10[8] holds
 * two s32 fade counters (elapsed at +0x10, duration at +0x14). */

/* Steps the world screen fade: ramps the menu brightness and the first fill box
 * colour toward the target until the elapsed counter reaches the duration. */
s32 wldcore_fade_step_screen(void) {
    u32 flags;
    s32 elapsed;
    s32 brightness;
    s32 box_level;

    flags = g_wldcore_screen_fade_state.flags[0];
    if (flags & 1) {
        if (g_wldcore_screen_fade_state.elapsed == g_wldcore_screen_fade_state.duration) {
            g_wldcore_screen_fade_state.flags[0] = flags & ~0xD;
        } else {
            if (g_wldcore_screen_fade_state.elapsed + 1 == g_wldcore_screen_fade_state.duration) {
                g_main_system_flags &= ~8;
                wldcore_sound_wait_for_queue_drain();
            }
            if (g_wldcore_screen_fade_state.flags[0] & 2) {
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
            g_wldcore_screen_brightness_rgb[0] = brightness;
            g_wldcore_screen_brightness_rgb[1] = brightness;
            g_wldcore_screen_brightness_rgb[2] = brightness;
            g_wldcore_screen_fade_state.boxes[0].r = box_level;
            g_wldcore_screen_fade_state.boxes[0].g = box_level;
            g_wldcore_screen_fade_state.boxes[0].b = box_level;
            if (g_wldcore_screen_fade_state.flags[0] & 4) {
                g_main_system_flags |= 2;
            }
            return 1;
        }
    }
    return 0;
}
