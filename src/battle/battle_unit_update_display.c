#include "fft/battle.h"

/* Apply a unit's pending status add/remove masks and refresh its display state. */
void battle_unit_update_display(battle_unit_misc_data_t* unit) {
    map_tile_t* tile;

    if (unit != 0) {
        tile = battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
        unit->status_flags_1_4 &= ~unit->statuses_to_remove_1_4;
        unit->status_flags_5_6 &= ~unit->statuses_to_remove_5_6;
        unit->status_flags_1_4 |= unit->statuses_to_add_1_4;
        unit->status_flags_5_6 |= unit->statuses_to_add_5_6;
        battle_unit_set_map_coords_after_death_dismount(unit);
        battle_gfx_update_sprite_transparency_flag(unit);
        battle_gfx_update_status_bubble_graphic_trigger(unit);
        if (g_battle_state_animation_continue_check != 0) {
            battle_gfx_check_tile_status_palette_mod(
                unit, (tile->flags_06.value >> MAP_TILE_SHADOW_MODE_SHIFT) & MAP_TILE_SHADOW_MODE_VALUE_MASK, 0);
        } else {
            battle_gfx_check_tile_status_palette_mod(
                unit, (tile->flags_06.value >> MAP_TILE_SHADOW_MODE_SHIFT) & MAP_TILE_SHADOW_MODE_VALUE_MASK, 1);
        }
        battle_unit_apply_poach_morbol_transformation(unit);
        battle_unit_set_animation_based_on_status(unit);
        battle_unit_update_animation_for_status_changes(unit);
        battle_gfx_apply_status_spritesheet_change(
            unit, (tile->flags_06.value >> MAP_TILE_SHADOW_MODE_SHIFT) & MAP_TILE_SHADOW_MODE_VALUE_MASK);
        battle_unit_update_float_and_jump_height(unit);
        unit->statuses_to_remove_5_6 = 0;
        unit->statuses_to_remove_1_4 = 0;
        unit->statuses_to_add_5_6 = 0;
        unit->statuses_to_add_1_4 = 0;
        unit->item_ability_display = 0;
    }
}
