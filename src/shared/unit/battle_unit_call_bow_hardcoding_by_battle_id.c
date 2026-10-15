#include "fft/battle_unit.h"

/* battle_unit_set_bow_hardcoding for two battle ids. No retail code calls it. */
void battle_unit_call_bow_hardcoding_by_battle_id(u16 attacker_id, u16 target_id) {
    battle_unit_t* attacker = battle_unit_get_by_battle_id(attacker_id);
    battle_unit_t* target = battle_unit_get_by_battle_id(target_id);

    battle_unit_set_bow_hardcoding(attacker, target);
}
