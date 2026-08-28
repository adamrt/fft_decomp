#include "fft/world.h"
#include "psx/types.h"

/* Menu-script command (table entry at 0x8018e010, beside
 * world_menu_script_set_draw_area): raise the window scale step, capped at 4,
 * and advance past the one-byte command. */
s32 world_menu_advance_window_scale_step(s32 script) {
    s8 step = g_world_menu_window_scale_step;

    if (step < 4) {
        g_world_menu_window_scale_step = step + 1;
    }
    return script + 1;
}
