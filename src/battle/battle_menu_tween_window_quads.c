#include "fft/battle.h"
#include "psx/types.h"

/* Animates a window's two quads from their current corners towards `to`
 * (and `to2` when it has an x) following the 12.4 fixed-point easing table
 * selected by `table`, yielding to the next thread between steps. Table 4
 * also fades the window colour with the remaining distance.
 *
 * BATTLE twin of world_menu_tween_window_quads. */
void battle_menu_tween_window_quads(menu_window_buffer_t* buffers, RECT* to, RECT* to2, s32 table) {
    RECT from;
    RECT from2;
    u16* easing;
    s32 i;
    s32 step;
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
    easing = g_battle_menu_window_tween_easing_tables[table];
    do {
        step = *easing;
        for (i = 0; i < 3 - g_battle_event_speed; i++) {
            fade = (ONE - (u32)step) >> 5;
            battle_thread_yield();
            if (table == 4) {
                if (*easing != ONE) {
                    buffer_index = g_battle_menu_packet_buffer_index;
                    buffers[buffer_index].b = fade;
                    buffers[buffer_index].g = fade;
                    buffers[buffer_index].r = fade;
                }
            }
            battle_menu_lerp_rect_to_poly_ft4(&buffers[g_battle_menu_packet_buffer_index].quads[0], &from, to, step);
            if (to2->x != 0) {
                battle_menu_lerp_rect_to_poly_ft4(
                    &buffers[g_battle_menu_packet_buffer_index].quads[1], &from2, to2, step);
            }
        }
    } while (*easing++ != ONE);
    battle_thread_yield();
    battle_menu_lerp_rect_to_poly_ft4(&buffers[g_battle_menu_packet_buffer_index].quads[0], &from, to, step);
    if (to2->x != 0) {
        battle_menu_lerp_rect_to_poly_ft4(&buffers[g_battle_menu_packet_buffer_index].quads[1], &from2, to2, step);
    }
}
