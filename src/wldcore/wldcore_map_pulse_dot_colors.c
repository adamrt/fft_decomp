/* The 0x40 compare limit reuses the dead `flags` local. With its own `limit`
 * variable the allocator swaps the g_wldcore_map_dot_pulse_direction snapshot ($a0 in the target) and
 * the limit ($v1); reusing the function-scope allocno reproduces the target
 * pair. */
#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* The dot's three colour bytes at +0x30, walked as its own advancing base. */
typedef struct map_dot_rgb {
    u8 r;
    u8 g;
    u8 b;
} map_dot_rgb_t;

void wldcore_map_pulse_dot_colors(void) {
    s32 flags;
    s32 i;
    s32 phase;
    s32 bright;
    s32 dark;
    u8 state;
    s32 counter;
    u8* color;

    flags = g_main_system_flags;
    if (flags & 0x400) {
        return;
    }
    if (!(flags & 0x8000000)) {
        state = g_wldcore_map_dot_pulse_direction;
        flags = 0x40;
        if (state == 0) {
            counter = g_wldcore_map_dot_pulse_phase + 1;
            g_wldcore_map_dot_pulse_phase = counter;
            counter = counter & 0xff;
            if (counter == flags) {
                g_wldcore_map_dot_pulse_direction = state ^ 1;
            }
        } else {
            counter = g_wldcore_map_dot_pulse_phase - 1;
            g_wldcore_map_dot_pulse_phase = counter;
            counter = counter & 0xff;
            if (counter == 0) {
                g_wldcore_map_dot_pulse_direction = state ^ 1;
            }
        }
    }
    phase = g_wldcore_map_dot_pulse_phase * 2;
    bright = phase + 0x60;
    dark = -0x20 - phase;
    for (i = 0; i < g_wldcore_map_dot_count; i++) {
        if (!(g_wldcore_map_dots[i].flags & 0x10)) {
            color = g_wldcore_map_dots[i].rgb;
            if (g_wldcore_map_dots[i].palette != 0) {
                color[0] = bright;
                color[1] = bright;
                color[2] = bright;
            } else {
                color[0] = dark;
                color[1] = dark;
                color[2] = dark;
            }
        }
    }
}
