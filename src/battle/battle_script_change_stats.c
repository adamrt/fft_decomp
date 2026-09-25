#include "fft/battle.h"
#include "psx/types.h"

/* Battle twin of world_script_change_stats. ChangeStats event instruction: add a signed delta to one stat of each
 * resolved unit.
 *
 * Parameters: unit id halfword, stat selector (0 HP, 2 CT, 3 Brave, 4 Faith), then a
 * halfword delta. HP clamps to max HP and the other stats to 0-100. */
void battle_script_change_stats(const u8* parameters) {
    u16 unit_id;
    s32 misc_index;
    s32 unit_index;
    s32 battle_id;
    s32 value;
    s16 delta;
    s32 battle_unit_index;
    battle_stats_t* unit;

    delta = battle_script_load_halfword(parameters + 3);
    unit_id = battle_script_load_halfword(parameters);
    if (battle_unit_try_get_misc_data_by_unit_id(&unit_id, &misc_index) != 0) {
        unit_index = 0;
        do {
            if (battle_script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &misc_index) != 0) {
                battle_unit_index = battle_unit_get_battle_index_by_misc_id(unit_id);
                if (battle_unit_index != -1) {
                    unit = battle_find_unit_data_pointer_for_entd_unit_id(
                        battle_unit_get_stats_from_battle_id(battle_unit_index)->unit_id, &battle_id);
                    if (battle_id >= 0) {
                        if (parameters[2] == 0) {
                            value = unit->hp + delta;
                            battle_script_clamp_s32_to_range(&value, 0, unit->max_hp);
                            unit->hp = value;
                        }
                        if (parameters[2] == 2) {
                            value = unit->ct + delta;
                            battle_script_clamp_s32_to_range(&value, 0, 100);
                            unit->ct = value;
                        }
                        if (parameters[2] == 3) {
                            value = unit->brave + delta;
                            battle_script_clamp_s32_to_range(&value, 0, 100);
                            unit->brave = value;
                        }
                        if (parameters[2] == 4) {
                            value = unit->faith + delta;
                            battle_script_clamp_s32_to_range(&value, 0, 100);
                            unit->faith = value;
                        }
                    }
                }
                if (misc_index == 0) {
                    break;
                }
            }
            unit_index++;
        } while (unit_index < EVENT_UNIT_SLOT_COUNT);
    }
}
