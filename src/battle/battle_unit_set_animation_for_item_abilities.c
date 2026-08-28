#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_set_animation_for_item_abilities(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* target) {
    s32 dx;
    s32 dy;

    battle_gfx_store_item_display_data((battle_gfx_render_unit_t*)unit, unit->used_item_or_weapon_id);
    if (target != 0) {
        dx = target->map_x - unit->map_x;
        dy = target->map_y - unit->map_y;
    } else {
        dx = 0;
        dy = 0;
    }
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    /* facing is read signed (lh). */
    if ((dx <= 0 && dy <= 0) || (dx < 2 && dy == 0) || (dx == 0 && dy < 2)) {
        battle_unit_store_animation_facing(0x39, *(s16*)&unit->facing, unit);
    } else {
        unit->item_ability_display = 0;
        battle_unit_store_animation_facing_movement_data(0x4C, *(s16*)&unit->facing, unit);
    }
}
