#include "fft/world.h"
#include "psx/types.h"

/* Ramp the tutorial highlight up to full brightness while the current command
 * is not the terminator, then redraw the highlight quads. Built with -O0 like
 * the other tutorial handlers. */
void world_script_advance_tutorial_highlight_brightness(void) {
    if (*g_world_script_tutorial_command_ptr != 0x14) {
        g_world_script_tutorial_wait_timer = 0x12;
        g_world_tutorial_highlight_quad_count += world_gfx_get_vsync_mode_or_one();
        if (g_world_tutorial_highlight_quad_count >= 0x1e) {
            g_world_tutorial_highlight_quad_count = 0x1e;
        }
    }
    world_script_draw_tutorial_highlight_quads(g_world_script_tutorial_wait_timer);
}
