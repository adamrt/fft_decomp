#include "fft/battle.h"
#include "fft/unit_slots.h"
#include "fft/wldcore.h"
#include "fft/world.h"

void world_script_blue_remove_all_enemy_units(void) {
    s32 resolved_id;
    s32 unit_index;
    s32 misc_id;

    misc_id = 0;
    do {
        if (battle_unit_has_misc_id(misc_id) != 0) {
            unit_index = wldcore_unit_get_id_from_misc_id(misc_id);
            if (unit_index != -1
                && (find_unit_by_id(battle_unit_get_stats_from_battle_id(unit_index)->unit_id, &resolved_id)
                           ->initial_team_flags
                       & 0x30)
                    != 0
                && resolved_id != -2) {
                battle_gfx_init_misc_unit_palette_modulation(misc_id, 2, -0x1f, -0x1f, 0);
            }
        }
        misc_id++;
    } while (misc_id < BATTLE_UNIT_SLOT_COUNT);
}
