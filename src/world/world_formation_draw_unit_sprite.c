#include "fft/world.h"
#include "psx/types.h"

/* Draws one formation unit sprite centerd on pos.
 *
 * When g_world_formation_dismiss_anim_timer selects this unit (high byte) the sprite is drawn shrunk by
 * (low byte + 1) without a shadow. Otherwise units with a proposition status
 * get a palette 9/11 backdrop first and the sprite is drawn with its shadow.
 * Compiled at -O1; the (s16) casts keep the centring offset from being
 * reassociated onto pos. */
void world_formation_draw_unit_sprite(s16 unit_index, point16_t* pos, rgb24_t* rgb, s32 ot_index) {
    oriented_quad_t sprite;
    s32 scale;
    s32 w;
    s32 h;
    rgb24_t* backdrop_rgb;

    world_formation_build_unit_graphic_entry(unit_index, &sprite.source, 0);
    if (g_world_formation_dismiss_anim_timer != 0 && (g_world_formation_dismiss_anim_timer >> 8) == unit_index) {
        scale = (g_world_formation_dismiss_anim_timer & 0xFF) + 1;
        w = sprite.source.w / scale;
        h = sprite.source.h / scale;
        sprite.position.x = pos->x - (s16)(w / 2 - 0x1C);
        sprite.position.y = pos->y - (s16)(h / 2 - 0x18);
        sprite.w = w;
        sprite.h = h;
        menu_gfx_enqueue_oriented_textured_quad(&sprite, rgb, TEXTURE_UV_FLIP_NONE, 0, ot_index);
        return;
    }
    sprite.position.x = pos->x - (s16)(sprite.source.w / 2 - 0x1F);
    sprite.position.y = pos->y - (s16)(sprite.source.h / 2 - 0x18);
    sprite.w = sprite.source.w;
    sprite.h = sprite.source.h;
    if (g_world_unit_pointers[unit_index]->proposition_status != 0) {
        if (unit_index == g_unit_selected_index) {
            backdrop_rgb = &g_world_formation_normal_sprite_color;
        } else {
            backdrop_rgb = menu_gfx_get_draw_color_variant();
        }
        world_formation_draw_graphics_below_sprite(unit_index,
            g_world_unit_pointers[unit_index]->proposition_status == WORLD_FORMATION_UNIT_PROPOSITION_LEAVING
                ? MENU_UNIT_READOUT_LEAVING
                : MENU_UNIT_READOUT_MISSING,
            pos->x, pos->y, backdrop_rgb, 0, 1, ot_index + 2);
        rgb = &g_world_formation_away_unit_color;
    }
    world_formation_draw_sprite_with_shadow(&sprite, rgb, ot_index);
}
