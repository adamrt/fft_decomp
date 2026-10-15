#include "fft/battle_unit.h"
#include "fft/world.h"
#include "psx/types.h"

/* Fills a status billboard (level, team kind, HP/MP/CT and the live-unit
 * count) from a battle unit. WORLD links this shorter body under the shared
 * name of src/shared/menu/menu_copy_unit_data_to_status_billboard_variant.c.
 *
 * Team kind is 0 for the player's side, 1 for the enemy team, 2 for a unit on
 * neither the enemy team nor under player control, and 3 for a unit on auto
 * battle. A zero maximum HP or MP is stored as 1. list_index is always 1
 * here; unit_count counts the runtime ids whose battle unit is the first holder
 * of a nonzero ENTD unit id. */
void menu_copy_unit_data_to_status_billboard_variant(unit_stats_t* unit, menu_unit_status_record_t* output) {
    s32 resolved_id;
    s32 count;
    s32 i;
    s32 unit_id;
    u16 value;
    u16 mp;
    u8 ct;

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
    if (value == 0) {
        value++;
        output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = value;
    }
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = 0;
    value = unit->max_mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = value;
    if (value == 0) {
        value++;
        output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = value;
    }
    mp = unit->mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = 0;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = mp;
    ct = unit->ct;
    output->list_index = 0;
    output->unit_count = 0;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = ct;
    if (output->list_index >= 0)
        output->list_index++;
    output->unit_count = BATTLE_UNIT_SLOT_COUNT;
    count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (battle_unit_has_runtime_id(i) != 0) {
            unit_id = battle_unit_get_battle_id_by_runtime_id(i);
            unit = battle_unit_find_stats_by_entd_unit_id(
                battle_unit_get_stats_from_battle_id(unit_id)->unit_id, &resolved_id);
            if (resolved_id >= 0 && unit_id == resolved_id && unit->unit_id != 0) {
                count++;
            }
        }
    }
    output->unit_count = count;
}
