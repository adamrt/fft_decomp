#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Draw an egg unit's two layered egg sprites and their shadow at pos.
 *
 * Callers pass the unit's egg_color as palette and the low nibble of birthday
 * (hatch countdown; see world_formation_get_unit_egg_hatch_progress) as
 * hatch_countdown; the second layer is drawn only while it is below 2.
 */
void world_formation_draw_unit_portrait_frame(
    world_gfx_sprite_desc_t* pos, s32 palette, s32 rgb, s32 hatch_countdown, s32 ot_index) {
    g_world_formation_shadow_egg_sprites[2].x = g_world_formation_shadow_egg_sprites[1].x = pos->x + 0x14;
    g_world_formation_shadow_egg_sprites[2].y = g_world_formation_shadow_egg_sprites[1].y = pos->y + 0x10;
    g_world_formation_shadow_egg_sprites[2].clut = g_world_formation_shadow_egg_sprites[1].clut
        = GetClut((palette % 4) * 0x10 + 0x140, 0xFF);
    world_gfx_enqueue_oriented_textured_quad(
        (const world_oriented_quad_t*)&g_world_formation_shadow_egg_sprites[1], (const u8*)rgb, 0, 0, ot_index);
    if (hatch_countdown < 2) {
        world_gfx_enqueue_oriented_textured_quad(
            (const world_oriented_quad_t*)&g_world_formation_shadow_egg_sprites[2], (const u8*)rgb, 0, 0, ot_index + 1);
    }
    g_world_formation_shadow_egg_sprites[0].x = pos->x + 0x13;
    g_world_formation_shadow_egg_sprites[0].y = pos->y + 0x23;
    g_world_formation_shadow_egg_sprites[0].tpage = GetTPage(0, 2, 0x3C0, 0x100);
    world_gfx_enqueue_oriented_textured_quad(
        (const world_oriented_quad_t*)&g_world_formation_shadow_egg_sprites[0], 0, 0, 1, ot_index - 1);
}
