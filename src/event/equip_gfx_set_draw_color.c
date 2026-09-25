#include "fft/event_equip.h"
#include "psx/types.h"

void equip_gfx_set_draw_color(const u8* data) {
    g_equip_gfx_sprite_color[0] = data[0];
    g_equip_gfx_sprite_color[1] = data[1];
    g_equip_gfx_sprite_color[2] = data[2];
}
