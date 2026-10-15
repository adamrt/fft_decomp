#include "fft/battle_gfx.h"

/* Refresh the status palette of a unit and of its mount partner, unless suppressed. */
void battle_gfx_check_tile_status_palette_mod(battle_unit_t* unit, s32 tile_effect_level, s32 final_value) {
    battle_unit_t* partner;

    if (!(unit->movement.suppress_palette_update)) {
        battle_gfx_update_unit_status_palette(unit, tile_effect_level, final_value);
    }
    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
        partner = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (partner != 0 && !(partner->movement.suppress_palette_update)) {
            battle_gfx_update_unit_status_palette(partner, tile_effect_level, final_value);
        }
    }
}
