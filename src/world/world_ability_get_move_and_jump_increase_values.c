#include "fft/world.h"

/*
 * Populate the Move and Jump bonuses granted by a movement ability.
 *
 * IDs 0x1E6-0x1E8 grant Move +1/+2/+3; 0x1E9-0x1EB grant Jump +1/+2/+3.
 * Other abilities leave both bonuses at 0.
 */
void world_ability_get_move_and_jump_increase_values(s16 ability, world_item_stat_detail_t* out) {
    world_item_stat_summary_t summary;

    world_formation_clear_stat_preview(&summary, out);
    if ((u16)(ability - ABILITY_ID_MOVEMENT_MOVE_PLUS_1) < 3) {
        out->move_bonus = ability - (ABILITY_ID_MOVEMENT_MOVE_PLUS_1 - 1);
    } else {
        out->move_bonus = 0;
    }
    if ((u16)(ability - ABILITY_ID_MOVEMENT_JUMP_PLUS_1) < 3) {
        out->jump_bonus = ability - (ABILITY_ID_MOVEMENT_JUMP_PLUS_1 - 1);
    } else {
        out->jump_bonus = 0;
    }
}
