#include "fft/attack.h"
#include "psx/types.h"

void attack_gfx_init_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* data) {
    data->rects[0].w = 0;
    data->rects[1].h = 0;
    battle_copy_bytes(&data->rects[1], &g_attack_gfx_draw_area_template, 8);
}
