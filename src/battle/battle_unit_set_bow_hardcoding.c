#include "fft/battle.h"

void battle_unit_set_bow_hardcoding(battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target) {
    if ((attacker != 0) && (target != 0)) {
        battle_sound_select_weapon_hit(attacker, 1);
        battle_unit_set_target_animation_from_attack_type(attacker, target);
        battle_gfx_prepare_post_action_display_by_misc_id(target->unit_id);
        battle_effect_set_evade_type_data_and_weapon_element(attacker, 1);
    }
}
