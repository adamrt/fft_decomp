#include "fft/battle.h"

/* Refresh the status palette of a unit and of its mount partner, unless suppressed. */
void battle_gfx_check_tile_status_palette_mod(battle_unit_misc_data_t* unit, s32 tile_effect_level, s32 final_value) {
    battle_unit_misc_data_t* partner;

    if (!(unit->movement.word & BATTLE_MOTION_FLAG_SUPPRESS_PALETTE_UPDATE)) {
        battle_gfx_update_misc_unit_status_palette(unit, tile_effect_level, final_value);
    }
    if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_NONE) {
        partner = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        if (partner != 0 && !(partner->movement.word & BATTLE_MOTION_FLAG_SUPPRESS_PALETTE_UPDATE)) {
            battle_gfx_update_misc_unit_status_palette(partner, tile_effect_level, final_value);
        }
    }
}
