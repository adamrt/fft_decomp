#include "fft/world.h"
#include "psx/types.h"

/* Formation sprite record: destination rectangle followed by the 12-byte
 * source entry filled by world_formation_build_unit_graphic_entry (so the
 * source clut/tpage land at 0x10/0x12, as world_gfx_sprite_desc_t expects). */
typedef struct world_formation_unit_sprite {
    s16 x;                                  /* 0x00 */
    s16 y;                                  /* 0x02 */
    s16 w;                                  /* 0x04 */
    s16 h;                                  /* 0x06 */
    world_formation_graphic_entry_t source; /* 0x08 */
} world_formation_unit_sprite_t;

/* Draws one formation unit sprite centred on pos.
 *
 * When g_world_formation_dismiss_anim_timer selects this unit (high byte) the sprite is drawn shrunk by
 * (low byte + 1) without a shadow. Otherwise units with a proposition status
 * get a palette 9/11 backdrop first and the sprite is drawn with its shadow.
 * Compiled at -O1; the (s16) casts keep the centring offset from being
 * reassociated onto pos. */
void world_formation_draw_unit_sprite(s16 unit_index, world_menu_point_t* pos, s8* rgb, s32 ot_index) {
    world_formation_unit_sprite_t sprite;
    s32 scale;
    s32 w;
    s32 h;
    u8* backdrop_rgb;

    world_formation_build_unit_graphic_entry(unit_index, &sprite.source, 0);
    if (g_world_formation_dismiss_anim_timer != 0 && (g_world_formation_dismiss_anim_timer >> 8) == unit_index) {
        scale = (g_world_formation_dismiss_anim_timer & 0xFF) + 1;
        w = (s16)sprite.source.w / scale;
        h = (s16)sprite.source.h / scale;
        sprite.x = pos->x - (s16)(w / 2 - 0x1C);
        sprite.y = pos->y - (s16)(h / 2 - 0x18);
        sprite.w = w;
        sprite.h = h;
        world_gfx_enqueue_oriented_textured_quad((const world_oriented_quad_t*)&sprite, (const u8*)rgb, 0, 0, ot_index);
        return;
    }
    sprite.x = pos->x - (s16)((s16)sprite.source.w / 2 - 0x1F);
    sprite.y = pos->y - (s16)((s16)sprite.source.h / 2 - 0x18);
    sprite.w = sprite.source.w;
    sprite.h = sprite.source.h;
    if (g_world_formation_unit_pointers[unit_index]->proposition_status != 0) {
        if (unit_index == g_world_formation_selected_unit_index) {
            backdrop_rgb = (u8*)g_world_formation_normal_sprite_color;
        } else {
            backdrop_rgb = (u8*)world_menu_get_sprite_color();
        }
        world_formation_draw_graphics_below_sprite(unit_index,
            g_world_formation_unit_pointers[unit_index]->proposition_status == 1 ? 9 : 0xB, pos->x, pos->y,
            backdrop_rgb, 0, 1, ot_index + 2);
        rgb = g_world_formation_away_unit_color;
    }
    world_formation_draw_sprite_with_shadow((world_gfx_sprite_desc_t*)&sprite, (s32)rgb, ot_index);
}
