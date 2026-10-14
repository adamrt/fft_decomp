#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw a formation unit sprite and, one ordering-table entry behind it, its
 * semi-transparent ground shadow: under the feet of a normal sprite, or
 * lower and further right for a sprite wider than 24 texels. */
void world_formation_draw_sprite_with_shadow(const oriented_quad_t* pos, const rgb24_t* rgb, s32 ot_index) {
    menu_gfx_enqueue_oriented_textured_quad(pos, rgb, TEXTURE_UV_FLIP_NONE, 0, ot_index);
    if (pos->source.w >= 0x19) {
        g_world_formation_shadow_egg_sprites[0].position.x = pos->position.x + 0xC;
        g_world_formation_shadow_egg_sprites[0].position.y = pos->position.y + 0x23;
    } else {
        g_world_formation_shadow_egg_sprites[0].position.x = pos->position.x;
        g_world_formation_shadow_egg_sprites[0].position.y = pos->position.y + 0x1E;
    }
    g_world_formation_shadow_egg_sprites[0].source.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    menu_gfx_enqueue_oriented_textured_quad(
        &g_world_formation_shadow_egg_sprites[0], 0, TEXTURE_UV_FLIP_NONE, 1, ot_index - 1);
}
