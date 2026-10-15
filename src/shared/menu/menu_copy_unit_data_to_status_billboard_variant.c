#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Fill the level, team, AT-list position, HP/MP/CT and live-unit count. */
void menu_copy_unit_data_to_status_billboard_variant(unit_stats_t* unit, menu_unit_status_record_t* output) {
    s32 resolved_id;
    s32 count;
    s32 i;
    s32 unit_id;
    u16 value;

    output->level = unit->level;
    output->team_kind = 0;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK)
        output->team_kind = 1;
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK))
        output->team_kind = 2;
    if (unit->auto_battle_setting != AI_AUTOBATTLE_MANUAL)
        output->team_kind = 3;
    output->experience = unit->experience;
    value = unit->max_hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = value;
    if (value == 0)
        output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = value + 1;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = 0;
    value = unit->max_mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = value;
    if (value == 0)
        output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = value + 1;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = unit->mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = 0;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = unit->ct;
    output->list_index = 0;
    output->unit_count = 0;
    g_thread_call_target = (void (*)(void))action_find_unit_turn_index;
    output->list_index = thread_call_on_main_stack(unit);
    if (unit->has_turn != 0)
        output->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
    if (output->list_index >= 0)
        output->list_index++;
    output->unit_count = BATTLE_UNIT_SLOT_COUNT;
    count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (battle_unit_has_runtime_id(i) != 0) {
            unit_id = battle_unit_get_battle_id_by_runtime_id(i);
            unit = battle_unit_get_stats_from_battle_id(unit_id);
            unit = battle_unit_find_stats_by_entd_unit_id(unit->unit_id, &resolved_id);
            if (resolved_id >= 0 && unit_id == resolved_id && unit->unit_id != 0) {
                count++;
            }
        }
    }
    output->unit_count = count;
}
