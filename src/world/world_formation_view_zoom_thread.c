#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Thread body: keeps the formation view zoomed and redrawn every frame until
 * the third thread parameter asks it to stop.
 */
void world_formation_view_zoom_thread(void) {
    s32 frame;

    if (g_world_formation_screen_active != 0) {
        g_world_formation_view.offset.position.x = 0;
        g_world_formation_view.offset.position.y = 8;
    } else {
        g_world_formation_view.offset.position.x = 0x100;
        g_world_formation_view.offset.position.y = 0x80;
    }
    if (thread_get_current_parameter_1_variant() == 0) {
        g_world_formation_view.scale_x = 0x2000;
        g_world_formation_view.scale_y = 0x2000;
    } else {
        g_world_formation_view.scale_x = ONE;
        g_world_formation_view.scale_y = ONE;
    }
    for (frame = 0;; frame++) {
        g_world_formation_view.ot = g_world_formation_ordering_table;
        if (g_world_formation_view.scale_x > ONE) {
            s32 step = g_script_event_speed << 8;
            g_world_formation_view.scale_x -= step;
            g_world_formation_view.scale_y -= step;
        }
        g_world_formation_view.brightness = g_world_menu_color_red;
        menu_build_zodiac_primitives(&g_world_formation_view, g_world_formation_view_quads[frame & 1]);
        thread_yield();
        if (thread_get_current_parameter_3_variant() != 0) {
            thread_exit_current();
        }
    }
}
