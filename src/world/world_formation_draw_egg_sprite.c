#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Draw an egg unit's two layered egg sprites and their shadow at pos.
 *
 * Callers pass the unit's egg_color as palette and the low nibble of birthday
 * (hatch countdown; see world_formation_get_unit_egg_hatch_progress) as
 * hatch_countdown; the second layer is drawn only while it is below 2.
 */
void world_formation_draw_egg_sprite(
    point16_t* pos, s32 palette, const rgb24_t* rgb, s32 hatch_countdown, s32 ot_index) {
    g_world_formation_shadow_egg_sprites[2].position.x = g_world_formation_shadow_egg_sprites[1].position.x
        = pos->x + 0x14;
    g_world_formation_shadow_egg_sprites[2].position.y = g_world_formation_shadow_egg_sprites[1].position.y
        = pos->y + 0x10;
    g_world_formation_shadow_egg_sprites[2].source.clut = g_world_formation_shadow_egg_sprites[1].source.clut
        = GetClut((palette % 4) * 0x10 + 0x140, 0xFF);
    menu_gfx_enqueue_oriented_textured_quad(
        &g_world_formation_shadow_egg_sprites[1], rgb, TEXTURE_UV_FLIP_NONE, 0, ot_index);
    if (hatch_countdown < 2) {
        menu_gfx_enqueue_oriented_textured_quad(
            &g_world_formation_shadow_egg_sprites[2], rgb, TEXTURE_UV_FLIP_NONE, 0, ot_index + 1);
    }
    g_world_formation_shadow_egg_sprites[0].position.x = pos->x + 0x13;
    g_world_formation_shadow_egg_sprites[0].position.y = pos->y + 0x23;
    g_world_formation_shadow_egg_sprites[0].source.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    menu_gfx_enqueue_oriented_textured_quad(
        &g_world_formation_shadow_egg_sprites[0], 0, TEXTURE_UV_FLIP_NONE, 1, ot_index - 1);
}
