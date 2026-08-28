#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_start_altima_teleport_fade_out(battle_unit_misc_data_t* unit) {
    map_tile_t* tile;

    if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_FIRST_FORM
        || unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM) {
        battle_effect_play();
    }
    tile = battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
    battle_gfx_check_tile_status_palette_mod(
        unit, (tile->flags_06.value >> MAP_TILE_SHADOW_MODE_SHIFT) & MAP_TILE_SHADOW_MODE_VALUE_MASK, 1);
    battle_gfx_start_misc_unit_palette_modulation(4, 0, unit->unit_id, -31, -31, -31);
    unit->distortion_phase++;
    unit->distortion_timer = 0;
}
