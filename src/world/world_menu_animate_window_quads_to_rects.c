#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Ease the current window's two quads from their present corners onto a pair
 * of destination rectangles. The interpolation parameter runs 0 -> 0x1000 in
 * decreasing steps, so the quads decelerate as they arrive; when the window is
 * growing (its quad area is smaller than the destination area) the sweep runs
 * a little past 0x1000 and then settles back down to it.
 */
void world_menu_animate_window_quads_to_rects(menu_window_buffer_t* buffers, RECT* to_main, RECT* to_icon) {
    RECT from_main;
    RECT from_icon;
    s32 quad_area;
    s32 target_area;
    s32 step;
    s32 limit;
    s32 t;
    s32 frame;

    from_main.x = buffers->quads[0].x0;
    from_main.y = buffers->quads[0].y0;
    from_main.w = buffers->quads[0].x3 - from_main.x;
    from_main.h = buffers->quads[0].y3 - from_main.y;
    quad_area = from_main.w * from_main.h;
    target_area = to_main->w * to_main->h;

    if (to_icon->x != 0) {
        from_icon.x = buffers->quads[1].x0;
        from_icon.y = buffers->quads[1].y0;
        from_icon.w = buffers->quads[1].x3 - from_icon.x;
        from_icon.h = buffers->quads[1].y3 - from_icon.y;
    }

    /* Setting step here, after the icon block, keeps its live range short
     * enough for it to outrank buffers for a callee-saved register. */
    step = 0x400;
    limit = ONE;
    if (quad_area < target_area) {
        step = 0x380;
    }
    /* Initialising t between the two guards keeps jump.c from merging them
     * and supplies the second branch's delay-slot instruction. */
    t = 0;
    if (quad_area < target_area) {
        limit = 0x1060;
    }

    for (; t <= limit; t += step) {
        for (frame = 0; frame < 3 - g_world_event_speed; frame++) {
            world_thread_yield();
            world_gfx_lerp_poly_ft4_rect_b(&buffers[g_world_menu_packet_buffer_index].quads[0], &from_main, to_main, t);
            if (to_icon->x != 0) {
                world_gfx_lerp_poly_ft4_rect_b(
                    &buffers[g_world_menu_packet_buffer_index].quads[1], &from_icon, to_icon, t);
            }
        }
        if (quad_area < target_area) {
            step -= 64;
            if (step < 64) {
                step = 64;
            }
        }
    }

    if (quad_area < target_area) {
        for (t = limit; t >= ONE; t -= 32) {
            for (frame = 0; frame < 3 - g_world_event_speed; frame++) {
                world_thread_yield();
                world_gfx_lerp_poly_ft4_rect_b(
                    &buffers[g_world_menu_packet_buffer_index].quads[0], &from_main, to_main, t);
                if (to_icon->x != 0) {
                    world_gfx_lerp_poly_ft4_rect_b(
                        &buffers[g_world_menu_packet_buffer_index].quads[1], &from_icon, to_icon, t);
                }
            }
        }
    }

    t = ONE;
    for (frame = 0; frame < 3 - g_world_event_speed; frame++) {
        world_thread_yield();
        world_gfx_lerp_poly_ft4_rect_b(&buffers[g_world_menu_packet_buffer_index].quads[0], &from_main, to_main, t);
        if (to_icon->x != 0) {
            world_gfx_lerp_poly_ft4_rect_b(&buffers[g_world_menu_packet_buffer_index].quads[1], &from_icon, to_icon, t);
        }
    }
}
