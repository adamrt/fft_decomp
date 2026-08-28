#include "fft/battle.h"

s32 battle_unit_set_target_anim_by_misc_id(u16 misc_id) {
    battle_unit_misc_data_t* target = battle_unit_get_misc_data_by_misc_id(misc_id);
    battle_unit_misc_data_t* caster = battle_unit_get_casting_misc_data();

    battle_unit_set_target_animation_from_attack_type(caster, target);
    return 1;
}
