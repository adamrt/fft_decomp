#include "fft/event_debugchr.h"
#include "psx/types.h"

void debugchr_gfx_init_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* packet) {
    packet->rects[0].w = 0;
    packet->rects[1].h = 0;
    battle_copy_bytes(&packet->rects[1], &g_debugchr_gfx_draw_area_template, 8);
}
