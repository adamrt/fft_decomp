#include "fft/battle_gfx.h"
#include "fft/main.h"

/* Queues the unit's item icon at its item-get offset from the unit's screen
 * position, at the unit's ordering-table depth and scale ONE. The x offset is
 * negated unless the sprite is flipped horizontally. */
void battle_gfx_init_item_get_rendering(battle_unit_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    u32* otag;

    if ((unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) != 0) {
        position.vx = unit->item_get_x_offset;
    } else {
        position.vx = -unit->item_get_x_offset;
    }
    position.vy = unit->item_get_y_offset;
    position.vx += unit->camera_relative_x;
    position.vy += unit->camera_relative_y;
    main_util_set_svector(&zoom, ONE, ONE, ONE);
    otag = main_gfx_get_otag();
    otag += unit->otag_depth_index;
    battle_gfx_construct_polygon_data_for_units(
        unit->item_display, unit->item_display->part_count, 0, &position, 0, 0, &zoom, otag);
}
