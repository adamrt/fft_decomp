#include "fft/battle.h"

void battle_gfx_init_item_get_rendering(battle_gfx_render_unit_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    u32* otag;

    if ((unit->sprite_flags & 2) != 0) {
        position.vx = (s16)unit->item_get_x_offset;
    } else {
        position.vx = -(s16)unit->item_get_x_offset;
    }
    position.vy = (s16)unit->item_get_y_offset;
    position.vx += unit->camera_relative_position.vx;
    position.vy += unit->camera_relative_position.vy;
    save_3_u16(&zoom, ONE, ONE, ONE);
    otag = main_gfx_get_otag();
    otag += unit->otag_depth;
    battle_gfx_construct_polygon_data_for_units((battle_gfx_sprite_display_t*)unit->found_item_display,
        unit->found_item_display->part_count, 0, (s16*)&position, 0, 0, (s16*)&zoom, otag);
}
