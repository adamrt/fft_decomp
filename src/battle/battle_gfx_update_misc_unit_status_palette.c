#include "fft/battle.h"
#include "psx/types.h"

/* The target passes the misc unit id in $a3 although the three-parameter
 * callee never reads it. */
typedef void (*battle_gfx_tint_4_fn)(battle_unit_misc_data_t*, s32, s32, s32);

/* Tint a unit's palette for its first palette-affecting status.
 *
 * Unless the unit already has a palette modifier, tints its CLUT for the
 * first matching renderer status (mode/RGB pairs below; Undead skips Skeleton
 * and Ghoul spritesheets); otherwise, or with no tinting status, it falls back
 * to battle_gfx_tint_unit_palette_for_weather_and_tile. */
void battle_gfx_update_misc_unit_status_palette(battle_unit_misc_data_t* misc, s32 tile_effect_level, s32 final_value) {
    u32 status;
    s32 unit_id;

    if (misc->palette_modifier != 0) {
        return;
    }
    status = misc->status_flags_1_4;
    unit_id = misc->unit_id;
    if (status & BATTLE_MISC_STATUS_PALETTE_MOD_MASK) {
        if (status & BATTLE_MISC_STATUS_PETRIFY) {
            battle_gfx_modify_misc_unit_palette(misc, 6, 3, unit_id, 0, 0, 0, 0, final_value);
        } else if (status & BATTLE_MISC_STATUS_BLOOD_SUCK) {
            battle_gfx_modify_misc_unit_palette(misc, 6, 3, unit_id, 0, 3, -1, 8, final_value);
        } else if (status & BATTLE_MISC_STATUS_BERSERK) {
            battle_gfx_modify_misc_unit_palette(misc, 4, 3, unit_id, 0, 8, 0, 0, final_value);
        } else if (status & BATTLE_MISC_STATUS_POISON) {
            battle_gfx_modify_misc_unit_palette(misc, 5, 3, unit_id, 0, 0, 8, 0, final_value);
        } else if (status & BATTLE_MISC_STATUS_REGEN) {
            battle_gfx_modify_misc_unit_palette(misc, 4, 3, unit_id, 0, 0, 0, 8, final_value);
        } else if (status & BATTLE_MISC_STATUS_OIL) {
            battle_gfx_modify_misc_unit_palette(misc, 5, 3, unit_id, 0, -4, -4, -4, final_value);
        } else if (status & BATTLE_MISC_STATUS_CURSED) {
            battle_gfx_modify_misc_unit_palette(misc, 5, 3, unit_id, 0, -8, -8, -8, final_value);
        } else if ((status & BATTLE_MISC_STATUS_UNDEAD)
            && (u8)(misc->spritesheet_id - BATTLE_SPRITESHEET_ID_SKELETON) >= 2) {
            battle_gfx_modify_misc_unit_palette(misc, 5, 3, unit_id, 0, 4, 0, 5, final_value);
        } else {
            ((battle_gfx_tint_4_fn)battle_gfx_tint_unit_palette_for_weather_and_tile)(
                misc, tile_effect_level, final_value, unit_id);
        }
        return;
    }
    ((battle_gfx_tint_4_fn)battle_gfx_tint_unit_palette_for_weather_and_tile)(
        misc, tile_effect_level, final_value, unit_id);
}
