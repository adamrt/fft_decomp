#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Store a unit's auto-battle target together with the resolved menu command as
 * its auto-battle setting. */
void menu_store_auto_battle_main_target_id(s32 target_id, s32 battle_id) {
    unit_stats_t* stats = battle_unit_get_stats_from_battle_id(battle_id);
    stats->auto_battle_target = target_id;
    stats->auto_battle_setting = g_menu_resolved_command;
}
