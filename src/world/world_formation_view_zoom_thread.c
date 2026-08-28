#include "fft/world.h"
#include "psx/types.h"

/*
 * Thread body: keeps the formation view zoomed and redrawn every frame until
 * the third thread parameter asks it to stop.
 */
void world_formation_view_zoom_thread(void) {
    s32 frame;

    if (g_world_formation_screen_active != 0) {
        g_world_formation_view.offset_x = 0;
        g_world_formation_view.offset_y = 8;
    } else {
        g_world_formation_view.offset_x = 0x100;
        g_world_formation_view.offset_y = 0x80;
    }
    if (world_thread_get_current_parameter_1() == 0) {
        g_world_formation_view.scale_x = 0x2000;
        g_world_formation_view.scale_y = 0x2000;
    } else {
        g_world_formation_view.scale_x = ONE;
        g_world_formation_view.scale_y = ONE;
    }
    for (frame = 0;; frame++) {
        g_world_formation_view.ordering_table = g_world_formation_ordering_table;
        if (g_world_formation_view.scale_x > ONE) {
            s32 step = g_world_event_speed << 8;
            g_world_formation_view.scale_x -= step;
            g_world_formation_view.scale_y -= step;
        }
        g_world_formation_view.color = g_world_menu_color_red;
        world_formation_build_view_primitives(
            &g_world_formation_view, (POLY_FT4*)g_world_formation_view_quads[frame & 1]);
        world_thread_yield();
        if (world_thread_get_current_parameter_3() != 0) {
            world_thread_exit_current();
        }
    }
}
