#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_dragon_heal_y_percent_add_status(void) {
    battle_formula_check_dragon();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_3c_damage_caster_max_hp_one_fifth_heal_target_two_fifths();
        battle_formula_apply_status();
    }
}
