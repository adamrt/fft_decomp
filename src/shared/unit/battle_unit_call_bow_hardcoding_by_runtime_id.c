#include "fft/battle_unit.h"

/* battle_unit_set_bow_hardcoding for two unit unit ids: called by the arrow-arc
 * and linear projectile effects for the unit the projectile hits. */
void battle_unit_call_bow_hardcoding_by_runtime_id(u16 attacker_id, u16 target_id) {
    battle_unit_t* attacker = battle_unit_get_by_runtime_id(attacker_id);
    battle_unit_t* target = battle_unit_get_by_runtime_id(target_id);

    battle_unit_set_bow_hardcoding(attacker, target);
}
