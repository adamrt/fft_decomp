#include "fft/battle.h"
#include "fft/battle_move.h"

/* Selects the slope bits of the two step tiles that face the movement
 * direction (top two bits of g_battle_move_step_value) and stores them as the unit's
 * per-step scales; a rider or a g_battle_move_step_value bit 0x10 step clears them. */
void battle_move_set_unit_step_slope_scales(battle_unit_misc_data_t* unit) {
    battle_move_get_current_and_destination_tiles(unit, &g_battle_move_current_tile, &g_battle_move_destination_tile);
    battle_gfx_check_tile_status_palette_mod(unit,
        (g_battle_move_destination_tile->flags_06.value >> MAP_TILE_SHADOW_MODE_SHIFT)
            & MAP_TILE_SHADOW_MODE_VALUE_MASK,
        1);
    switch (g_battle_move_step_value >> 6) {
    case 0:
        unit->destination_edge_height = g_battle_move_destination_edge_height
            = (g_battle_move_destination_tile->slope_type & 0xC) >> 2;
        unit->current_edge_height = g_battle_move_current_edge_height = g_battle_move_current_tile->slope_type & 3;
        break;
    case 1:
        unit->destination_edge_height = g_battle_move_destination_edge_height
            = g_battle_move_destination_tile->slope_type & 3;
        unit->current_edge_height = g_battle_move_current_edge_height
            = (g_battle_move_current_tile->slope_type & 0xC) >> 2;
        break;
    case 2:
        unit->destination_edge_height = g_battle_move_destination_edge_height
            = g_battle_move_destination_tile->slope_type >> 6;
        unit->current_edge_height = g_battle_move_current_edge_height
            = (g_battle_move_current_tile->slope_type & 0x30) >> 4;
        break;
    case 3:
        unit->destination_edge_height = g_battle_move_destination_edge_height
            = (g_battle_move_destination_tile->slope_type & 0x30) >> 4;
        unit->current_edge_height = g_battle_move_current_edge_height = g_battle_move_current_tile->slope_type >> 6;
        break;
    }
    if (unit->current_unit_id_plus_one != 0) {
        unit->current_edge_height = g_battle_move_current_edge_height = 0;
    }
    if (g_battle_move_step_value & 0x10) {
        unit->destination_edge_height = g_battle_move_destination_edge_height = 0;
    }
}
