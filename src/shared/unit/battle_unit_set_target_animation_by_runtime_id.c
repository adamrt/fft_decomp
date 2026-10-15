#include "fft/battle_unit.h"

/* Starts a unit's reaction animation to the casting unit's action
 * (battle_unit_set_target_animation_from_attack_type). Always returns 1. */
s32 battle_unit_set_target_animation_by_runtime_id(s32 runtime_id) {
    battle_unit_t* target = battle_unit_get_by_runtime_id(runtime_id);
    battle_unit_t* caster = battle_unit_get_casting();

    battle_unit_set_target_animation_from_attack_type(caster, target);
    return 1;
}
