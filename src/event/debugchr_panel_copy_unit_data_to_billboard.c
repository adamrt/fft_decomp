#include "fft/event_debugchr.h"
#include "psx/types.h"

/* Fills a status billboard (level, team kind, AT position, HP/MP/CT and the
 * live-unit count) from a battle unit.
 *
 * DEBUGCHR twin of battle_menu_copy_unit_data_to_status_billboard, with the
 * same layout. Reusing `unit` for the loop's stats and lookup pointers ranks
 * it ahead of `i` for $s0, and reading MP directly rather than through `value`
 * gives CT $v1 and the callback address $v0 in the call block. */
void debugchr_panel_copy_unit_data_to_billboard(battle_stats_t* unit, u8* buffer) {
    battle_unit_status_record_t* output;
    s32 resolved_id;
    s32 count;
    s32 i;
    s32 unit_id;
    u16 value;

    output = (battle_unit_status_record_t*)buffer;
    output->level = unit->level;
    output->team_kind = 0;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK)
        output->team_kind = 1;
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK))
        output->team_kind = 2;
    if (unit->auto_battle_setting != 0)
        output->team_kind = 3;
    output->experience = unit->experience;
    value = unit->max_hp;
    output->gauges[BATTLE_UNIT_STATUS_GAUGE_HP].max = value;
    if (value == 0)
        output->gauges[BATTLE_UNIT_STATUS_GAUGE_HP].max = value + 1;
    output->gauges[BATTLE_UNIT_STATUS_GAUGE_HP].value = unit->hp;
    output->gauges[BATTLE_UNIT_STATUS_GAUGE_HP].value2 = 0;
    value = unit->max_mp;
    output->gauges[BATTLE_UNIT_STATUS_GAUGE_MP].max = value;
    if (value == 0)
        output->gauges[BATTLE_UNIT_STATUS_GAUGE_MP].max = value + 1;
    output->gauges[BATTLE_UNIT_STATUS_GAUGE_MP].value = unit->mp;
    output->gauges[BATTLE_UNIT_STATUS_GAUGE_MP].value2 = 0;
    output->gauges[BATTLE_UNIT_STATUS_GAUGE_CT].max = 100;
    output->gauges[BATTLE_UNIT_STATUS_GAUGE_CT].value = unit->ct;
    output->list_index = 0;
    output->unit_count = 0;
    g_battle_thread_call_target = (void (*)(void))battle_action_check_at_list_for_unit_battle_id;
    output->list_index = battle_thread_call_on_main_stack(unit);
    if (unit->has_turn != 0)
        output->gauges[BATTLE_UNIT_STATUS_GAUGE_CT].value = 100;
    if (output->list_index >= 0)
        output->list_index++;
    output->unit_count = BATTLE_UNIT_SLOT_COUNT;
    count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (battle_unit_has_misc_id(i) != 0) {
            unit_id = battle_unit_get_battle_index_by_misc_id(i);
            unit = battle_unit_get_stats_from_battle_id(unit_id);
            unit = battle_find_unit_data_pointer_for_entd_unit_id(unit->unit_id, &resolved_id);
            if (resolved_id >= 0 && unit_id == resolved_id && unit->unit_id != 0) {
                count++;
            }
        }
    }
    output->unit_count = count;
}
