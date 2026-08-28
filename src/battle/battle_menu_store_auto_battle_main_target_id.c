#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_store_auto_battle_main_target_id(s32 target_id, s32 battle_id) {
    battle_stats_t* stats = battle_unit_get_stats_from_battle_id(battle_id);
    stats->auto_battle_target = target_id;
    stats->auto_battle_setting = (u8)g_battle_menu_resolved_command;
}
