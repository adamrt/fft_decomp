#include "fft/battle.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Start the second animation of the used ability's charge set, the one held
 * while charging or performing; the submerged idle in water two or more deep. */
void battle_unit_start_ability_charge_hold_animation(battle_unit_t* unit) {
    u8 animation;
    u8 water_depth;
    s32 selected_animation;

    animation = g_battle_ability_charge_animation_sets[g_battle_ability_animation_data[unit->used_ability_id]
            .charge_animation_set_id][1];
    if (animation != 0) {
        water_depth = move_get_water_depth(unit);
        selected_animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
        if (water_depth < 2) {
            selected_animation = animation;
        }
        /* facing is read signed (lh) here. */
        battle_unit_store_animation_facing(selected_animation, unit->facing, unit);
    }
}
