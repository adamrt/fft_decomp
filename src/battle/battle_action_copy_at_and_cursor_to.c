#include "fft/battle.h"
#include "psx/types.h"

/* Snapshot the AT banner, active unit, billboard and cursor tile records into
   the four caller-owned buffers so the caller can restore them later. */
void battle_action_copy_at_and_cursor_to(void* banner, void* unit, void* billboard, void* cursor_tile) {
    battle_copy_bytes(banner, &g_battle_menu_active_turn_banner, sizeof(g_battle_menu_active_turn_banner));
    battle_copy_bytes(unit, &g_battle_active_turn_unit, sizeof(g_battle_active_turn_unit));
    battle_copy_bytes(billboard, g_battle_menu_billboard_data, sizeof(g_battle_menu_billboard_data));
    battle_copy_bytes(cursor_tile, &g_battle_map_selected_tile_data, sizeof(g_battle_map_selected_tile_data));
}
