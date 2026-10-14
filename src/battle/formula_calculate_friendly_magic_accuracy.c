#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "psx/types.h"

/* Hit chance F(XA+YA)% for magic on allies: Magic Attack Up and zodiac
 * compatibility, none of the target's defenses. Returns 1 on a miss. */
s32 formula_calculate_friendly_magic_accuracy(void) {
    formula_apply_magic_attack_up();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya_status_damage();
    formula_calculate_faith();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}
