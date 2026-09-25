#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_start_ability_charge_animation_for_movement(battle_unit_misc_data_t* unit) {
    u8 animation;
    u8 movement_type;
    s32 selected_animation;

    animation = g_battle_ability_charge_animation_sets[g_battle_ability_animation_data[unit->used_ability_id]
            .charge_animation_set_id][1];
    if (animation != 0) {
        movement_type = battle_move_validate_float_fly(unit);
        selected_animation = 9;
        if (movement_type < 2) {
            selected_animation = animation;
        }
        /* facing is read signed (lh) here. */
        battle_unit_store_animation_facing(selected_animation, *(s16*)&unit->facing, unit);
    }
}
