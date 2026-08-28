#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

void world_formation_draw_sprite_with_shadow(world_gfx_sprite_desc_t* pos, s32 rgb, s32 ot_index) {
    world_gfx_enqueue_oriented_textured_quad((const world_oriented_quad_t*)pos, (const u8*)rgb, 0, 0, ot_index);
    if (pos->texture_width >= 0x19) {
        g_world_formation_shadow_egg_sprites[0].x = pos->x + 0xC;
        g_world_formation_shadow_egg_sprites[0].y = pos->y + 0x23;
    } else {
        g_world_formation_shadow_egg_sprites[0].x = pos->x;
        g_world_formation_shadow_egg_sprites[0].y = pos->y + 0x1E;
    }
    g_world_formation_shadow_egg_sprites[0].tpage = GetTPage(0, 2, 0x3C0, 0x100);
    world_gfx_enqueue_oriented_textured_quad(
        (const world_oriented_quad_t*)&g_world_formation_shadow_egg_sprites[0], 0, 0, 1, ot_index - 1);
}
