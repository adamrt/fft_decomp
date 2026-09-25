#include "fft/wldcore.h"
#include "psx/gpu.h"

void wldcore_window_load_party_unit_sprite_record(wldcore_menu_send_unit_level_t* unused, s32 party_index, s32 index) {
    POLY_FT4 poly;
    s32 code;
    s32 cell;
    wldcore_window_entry_52_rgb_t* color;
    wldcore_window_render_bounds16_t* bounds;

    code = world_get_party_unit_formation_sprite(party_index);
    cell = code;
    world_gfx_set_portrait_poly_texture(&poly, code);
    g_wldcore_window_render_records[index].clut_x = (code / 40 << 6) + 0x280 + cell % 4 * 16;
    g_wldcore_window_render_records[index].clut_y = cell % 40 / 4 + 0x1F0;
    g_wldcore_window_render_records[index].tpage = poly.tpage;
    bounds = (wldcore_window_render_bounds16_t*)&g_wldcore_window_render_records[index].x;
    bounds->position.x = poly.u0;
    bounds->position.y = poly.v0;
    bounds->dimensions.x = poly.u1 - poly.u0;
    bounds->dimensions.y = poly.v2 - poly.v0;
    color = (wldcore_window_entry_52_rgb_t*)&g_wldcore_window_render_records[index].red;
    color->red = 0x80;
    color->green = 0x80;
    color->blue = 0x80;
    g_wldcore_window_render_records[index].anim_counter = 0;
    g_wldcore_window_render_records[index].flags &= ~0x10;
}
