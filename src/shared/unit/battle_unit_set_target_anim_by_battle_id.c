#include "fft/battle_unit.h"

/* Starts the reaction animation of the unit in slot battle_id to the casting
 * unit's action. Always returns 1. No retail code calls it. */
s32 battle_unit_set_target_anim_by_battle_id(u16 battle_id) {
    battle_unit_t* target = battle_unit_get_by_battle_id(battle_id);
    battle_unit_t* caster = battle_unit_get_casting();

    battle_unit_set_target_animation_from_attack_type(caster, target);
    return 1;
}
