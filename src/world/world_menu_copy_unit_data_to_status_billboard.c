#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_copy_unit_data_to_status_billboard(battle_stats_t* unit, world_unit_status_billboard_t* output) {
    battle_stats_t* linked_unit;
    s32 resolved_id;
    s32 count;
    s32 i;
    s32 unit_id;
    u16 value;
    u16 mp;
    u8 ct;

    linked_unit = unit;
    output->level = linked_unit->level;
    output->team_kind = 0;
    if (linked_unit->initial_team_flags & BATTLE_TEAM_MASK)
        output->team_kind = 1;
    if (!(linked_unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK))
        output->team_kind = 2;
    if (linked_unit->auto_battle_setting != 0)
        output->team_kind = 3;
    output->experience = linked_unit->experience;
    value = linked_unit->max_hp;
    output->max_hp = value;
    if (value == 0) {
        value++;
        output->max_hp = value;
    }
    output->hp = linked_unit->hp;
    output->hp_delta = 0;
    value = linked_unit->max_mp;
    output->max_mp = value;
    if (value == 0) {
        value++;
        output->max_mp = value;
    }
    mp = linked_unit->mp;
    output->mp_delta = 0;
    output->max_ct = 100;
    output->mp = mp;
    ct = linked_unit->ct;
    output->list_index = 0;
    output->unit_count = 0;
    output->ct = ct;
    if (output->list_index >= 0)
        output->list_index++;
    output->unit_count = BATTLE_UNIT_SLOT_COUNT;
    count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (battle_unit_has_misc_id(i) != 0) {
            unit_id = battle_unit_get_battle_index_by_misc_id(i);
            linked_unit = battle_find_unit_data_pointer_for_entd_unit_id(
                battle_unit_get_stats_from_battle_id(unit_id)->unit_id, &resolved_id);
            if (resolved_id >= 0 && unit_id == resolved_id && linked_unit->unit_id != 0) {
                count++;
            }
        }
    }
    output->unit_count = count;
}
