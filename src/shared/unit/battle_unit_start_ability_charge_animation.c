#include "fft/battle.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Start the first animation of the used ability's charge set, if it has one. */
void battle_unit_start_ability_charge_animation(battle_unit_t* unit) {
    u8 animation;

    animation = g_battle_ability_charge_animation_sets[g_battle_ability_animation_data[unit->used_ability_id]
            .charge_animation_set_id][0];
    if (animation != 0) {
        /* facing is passed sign-extended (lh) here. */
        battle_unit_store_animation_facing(animation, unit->facing, unit);
    }
}
