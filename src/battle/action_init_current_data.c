#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Start resolving a unit's queued action: check and spend its MP, record it as
 * the acting unit with its last ability, clear every unit's reaction and outcome
 * and set up the first strike.
 *
 * Returns 0 when the action can run, -1 for an empty unit slot, 6 when Hamedo
 * pre-empted the action (g_action_hamedo_flag set), or the nonzero
 * rejection code of action_check_and_consume_mp; on those failures only
 * the acting-unit data is stored. */
s32 action_init_current_data(s32 id) {
    unit_stats_t* unit;
    s32 i;
    unit_stats_t* base;
    s32 mp_result;
    u16 ability;

    g_battle_acting_unit_id = -1;
    g_action_context = ACTION_CONTEXT_PRIMARY;
    if (id >= BATTLE_UNIT_SLOT_COUNT
        || (unit = &g_battle_unit_stats[id], base = g_battle_unit_stats, unit->entd_slot == BATTLE_ENTD_SLOT_NONE)) {
        return -1;
    }
    if (g_action_hamedo_flag != 0) {
        action_store_acting_unit_data(unit);
        return 6;
    }
    mp_result = action_check_and_consume_mp(unit);
    if (mp_result != 0) {
        action_store_acting_unit_data(unit);
        return mp_result;
    }
    /* The target loads the s16 action_command.ability_id unsigned (lhu). */
    ability = unit->action_command.ability_id;
    g_battle_acting_unit_id = id;
    g_battle_acting_unit_used_ability_id = ability;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        base[i].action.reaction_id = 0;
        base[i].ability_outcome = 0;
    }
    action_init_current_ability_strike_data(unit);
    action_set_current_attacker_data(unit);
    return 0;
}
