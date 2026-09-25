#include "fft/attack.h"
#include "fft/battle_menu_status_panel.h"
#include "fft/event.h"
#include "psx/gpu.h"
#include "psx/types.h"

void attack_gfx_build_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* packet,
    const void* source, s32 scale_index, s32 lower_half, const s16* offset) {
    s32 percent;
    s32 w;
    s32 h;
    s32 scaled;

    if (g_battle_event_speed == 2) {
        scale_index *= 2;
    }
    if (scale_index >= 12) {
        scale_index = 11;
    }
    percent = g_attack_gfx_draw_area_scale_percent[scale_index];
    battle_copy_bytes(&packet->rects[0], source, 8);
    w = packet->rects[0].w;
    h = packet->rects[0].h;
    packet->rects[0].x -= 0x80;
    packet->rects[0].x += offset[0];
    packet->rects[0].y += offset[2];
    scaled = (w * percent) / 200;
    packet->rects[0].x = w / 2 + packet->rects[0].x - scaled;
    scaled = (h * percent) / 200;
    packet->rects[0].y = h / 2 + packet->rects[0].y - scaled;
    packet->rects[0].w = (packet->rects[0].w * percent) / 100;
    packet->rects[0].h = (packet->rects[0].h * percent) / 100;
    if (lower_half != 0) {
        packet->rects[1].y = 0;
    } else {
        packet->rects[1].y = 0xF0;
        packet->rects[0].y += 0xF0;
    }
    SetDrawArea(&packet->areas[0], &packet->rects[0]);
    SetDrawArea(&packet->areas[1], &packet->rects[1]);
}
