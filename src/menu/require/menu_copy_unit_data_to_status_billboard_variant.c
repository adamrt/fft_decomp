#include "fft/event_require.h"
#include "psx/types.h"

/* Fills a status billboard (level, team kind, experience and the HP/MP/CT
 * gauges) from a battle unit, leaving the unit index, list index and unit
 * count 0.
 *
 * Team kind is 0 for the player's side, 1 for the enemy team, 2 for a unit on
 * neither the enemy team nor under player control, and 3 for a unit on auto
 * battle. A zero maximum HP or MP is stored as 1. */
void menu_copy_unit_data_to_status_billboard_variant(unit_stats_t* unit, menu_unit_status_record_t* output) {
    u16 maximum_hp;
    u16 maximum_mp;
    u8 ct;

    output->level = unit->level;
    output->team_kind = 0;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
        output->team_kind = 1;
    }
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        output->team_kind = 2;
    }
    if (unit->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
        output->team_kind = 3;
    }
    output->experience = unit->experience;
    maximum_hp = unit->max_hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp;
    if (maximum_hp == 0) {
        output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp + 1;
    }
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = 0;
    maximum_mp = unit->max_mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp;
    if (maximum_mp == 0) {
        output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp + 1;
    }
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = unit->mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = 0;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    ct = unit->ct;
    output->unit_index = 0;
    output->list_index = 0;
    output->unit_count = 0;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = ct;
}
