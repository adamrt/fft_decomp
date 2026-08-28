#include "fft/attack.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Thread body that animates the map-title panel strip with double-buffered
 * primitives: widen it by the thread's step, hold it, then sweep it again
 * with `reverse` set. The hold phase reuses `width` as its frame counter. */
void attack_map_show_title(void) {
    s32 step;
    s32 buffer;
    s32 width;

    step = battle_thread_get_current_parameter_3();
    buffer = 0;
    if (step == 0) {
        step = 1;
    }

    width = 8;
    do {
        buffer ^= 1;
        attack_gfx_build_shaded_panel_strips(g_attack_map_title_sprites[buffer], width, 0x80, 0);
        width += step;
        battle_thread_yield();
    } while (width < 0xf8);

    width = 0;
    do {
        buffer ^= 1;
        attack_gfx_build_shaded_panel_strips(g_attack_map_title_sprites[buffer], 0xf8, 0x80, 0);
        width += step;
        battle_thread_yield();
    } while (width < 0x6e);

    width = 8;
    do {
        buffer ^= 1;
        attack_gfx_build_shaded_panel_strips(g_attack_map_title_sprites[buffer], width, 0x80, 1);
        if (width >= 0x81) {
            g_attack_map_title_closing_past_midpoint = 1;
        }
        width += step;
        battle_thread_yield();
    } while (width < 0xf9);

    battle_thread_exit_current();
}
