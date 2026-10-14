#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Move both window quads through the easing table; table 4 also fades their color. */
void menu_tween_window_quads(menu_window_buffer_t* buffers, RECT* to, RECT* to2, s32 table) {
    RECT from;
    RECT from2;
    u16* easing;
    s32 i;
    u32 step;
    s32 fade;
    u16 x;
    u16 y;
    s32 buffer_index;

    x = buffers->quads[0].x0;
    from.x = x;
    y = buffers->quads[0].y0;
    from.y = y;
    from.w = buffers->quads[0].x3 - x;
    from.h = buffers->quads[0].y3 - y;
    if (to2->x != 0) {
        x = buffers->quads[1].x0;
        from2.x = x;
        y = buffers->quads[1].y0;
        from2.y = y;
        from2.w = buffers->quads[1].x3 - x;
        from2.h = buffers->quads[1].y3 - y;
    }
    easing = g_menu_window_tween_easing_tables[table];
    do {
        step = *easing;
        for (i = 0; i < 3 - g_script_event_speed; i++) {
            fade = (ONE - step) >> 5;
            thread_yield();
            if (table == 4) {
                if (*easing != ONE) {
                    buffer_index = g_menu_packet_buffer_index;
                    buffers[buffer_index].color.b = fade;
                    buffers[buffer_index].color.g = fade;
                    buffers[buffer_index].color.r = fade;
                }
            }
            menu_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[0], &from, to, step);
            if (to2->x != 0) {
                menu_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[1], &from2, to2, step);
            }
        }
    } while (*easing++ != ONE);
    thread_yield();
    menu_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[0], &from, to, step);
    if (to2->x != 0) {
        menu_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[1], &from2, to2, step);
    }
}
