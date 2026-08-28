#include "fft/battle_ai.h"

/*
 * Populate the AI block acting-unit fields for the unit whose turn is being prepared.
 *
 * A mounted rider uses its mount's movement allowance.
 */
void battle_ai_init_acting_unit_data(void) {
    battle_ai_data_t* ai;
    battle_stats_t* unit;
    battle_stats_t* acting;
    s32 mount_info;
    s32 id;

    ai = &g_battle_ai_data_base;
    id = g_battle_ai_data_base.acting_unit_id;
    unit = &g_battle_unit_stats[id];
    g_battle_ai_temp_unit_data = unit;
    g_battle_ai_data_base.acting_unit_decision = &ai->unit_decisions[id];
    g_battle_ai_data_base.acting_unit_coords.bytes.x = unit->x;
    g_battle_ai_data_base.acting_unit_coords.bytes.y = unit->position.bits.y;
    g_battle_ai_data_base.acting_unit_coords.bytes.elevation = unit->position.bits.higher_elevation;
    g_battle_ai_data_base.acting_unit_coords.bytes.zero = 0;
    g_battle_ai_data_base.acting_unit_battle_id = ai->unit_battle_ids[id];
    g_battle_ai_data_base.acting_unit_team = unit->team_flags & BATTLE_TEAM_MASK;
    g_battle_ai_acting_unit_remaining_clockticks = battle_ai_calculate_clockticks_until_unit_acts(unit);
    acting = g_battle_ai_temp_unit_data;
    mount_info = acting->mount_info;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        g_battle_ai_data_base.acting_unit_move
            = g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK].move;
    } else {
        g_battle_ai_data_base.acting_unit_move = acting->move;
    }
    ai->current_action.coords.bytes.zero = 0;
    ai->current_action.unit_id = ai->acting_unit_id;
}
