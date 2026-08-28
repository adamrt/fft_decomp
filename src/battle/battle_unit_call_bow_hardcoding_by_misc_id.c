#include "fft/battle.h"

void battle_unit_call_bow_hardcoding_by_misc_id(u16 attacker_id, u16 target_id) {
    battle_unit_misc_data_t* attacker = battle_unit_get_misc_data_by_misc_id(attacker_id);
    battle_unit_misc_data_t* target = battle_unit_get_misc_data_by_misc_id(target_id);

    battle_unit_set_bow_hardcoding(attacker, target);
}
