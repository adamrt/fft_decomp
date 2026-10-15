#include "fft/world.h"

/* Extend the tutorial highlight while other commands run; WAIT counts its brightness down. */
void world_script_advance_tutorial_highlight_brightness(void) {
    if (*g_world_script_tutorial_command_ptr != WORLD_TUTORIAL_CMD_WAIT) {
        g_world_script_tutorial_wait_timer = 0x12;
        g_world_tutorial_highlight_quad_count += menu_get_event_speed();
        if (g_world_tutorial_highlight_quad_count >= 0x1e) {
            g_world_tutorial_highlight_quad_count = 0x1e;
        }
    }
    world_script_draw_tutorial_highlight_quads(g_world_script_tutorial_wait_timer);
}
