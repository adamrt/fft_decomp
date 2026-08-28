#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

void battle_action_copy_active_turn_data_from(const u8* banner, const u8* unit, const u8* billboard) {
    battle_copy_bytes(&g_battle_menu_active_turn_banner, banner, sizeof(g_battle_menu_active_turn_banner));
    battle_copy_bytes(&g_battle_active_turn_unit, unit, sizeof(g_battle_active_turn_unit));
    battle_copy_bytes(g_battle_menu_billboard_data, billboard, sizeof(g_battle_menu_billboard_data));
}
