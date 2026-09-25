#include "fft/battle.h"

/* BlueRemoveUnit (event instruction 0x99): tints every non-player unit whose
 * ENTD unit id is unit_id blue (red and green modulated by -0x1f).
 *
 * The team test reads the current team byte (0x05), not the initial one at
 * 0x1ba. */
void battle_script_blue_remove_unit(s32 unit_id) {
    s32 misc_id;
    s32 battle_index;
    battle_stats_t* unit;
    s32 found_battle_id[2];

    for (misc_id = 0; misc_id < EVENT_UNIT_SLOT_COUNT; misc_id++) {
        if (battle_unit_has_misc_id(misc_id) == 0) {
            continue;
        }
        battle_index = battle_unit_get_battle_index_by_misc_id(misc_id);
        if (battle_index == -1) {
            continue;
        }
        unit = battle_find_unit_data_pointer_for_entd_unit_id(
            battle_unit_get_stats_from_battle_id(battle_index)->unit_id, found_battle_id);
        if ((unit->team_flags & BATTLE_TEAM_MASK) == 0) {
            continue;
        }
        if (found_battle_id[0] == -2) {
            continue;
        }
        if (unit->unit_id != unit_id) {
            continue;
        }
        battle_gfx_init_misc_unit_palette_modulation(misc_id, 2, -0x1F, -0x1F, 0);
    }
}
