#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Tint a unit's palette for its first palette-affecting status.
 *
 * Unless the unit already has a palette modifier, tints its CLUT for the
 * first matching renderer status (mode/RGB pairs below; Undead skips Skeleton
 * and Ghoul spritesheets); otherwise, or with no tinting status, it falls back
 * to battle_gfx_tint_unit_palette_for_weather_and_tile. */
void battle_gfx_update_unit_status_palette(battle_unit_t* unit, s32 tile_effect_level, s32 final_value) {
    u32 status;
    s32 unit_id;

    if (unit->fade_out_phase != BATTLE_UNIT_FADE_OUT_NONE) {
        return;
    }
    status = unit->status_flags_1_4;
    unit_id = unit->runtime_id;
    if (status & BATTLE_UNIT_STATUS_PALETTE_MOD_MASK) {
        if (status & BATTLE_UNIT_STATUS_PETRIFY) {
            battle_gfx_modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_GRAY, MAP_CLUT_ROW_UNITS, unit_id, 0, 0, 0, 0, final_value);
        } else if (status & BATTLE_UNIT_STATUS_BLOOD_SUCK) {
            battle_gfx_modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_GRAY, MAP_CLUT_ROW_UNITS, unit_id, 0, 3, -1, 8, final_value);
        } else if (status & BATTLE_UNIT_STATUS_BERSERK) {
            battle_gfx_modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE, MAP_CLUT_ROW_UNITS, unit_id, 0, 8, 0, 0, final_value);
        } else if (status & BATTLE_UNIT_STATUS_POISON) {
            battle_gfx_modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, 0, 8, 0, final_value);
        } else if (status & BATTLE_UNIT_STATUS_REGEN) {
            battle_gfx_modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE, MAP_CLUT_ROW_UNITS, unit_id, 0, 0, 0, 8, final_value);
        } else if (status & BATTLE_UNIT_STATUS_OIL) {
            battle_gfx_modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, -4, -4, -4, final_value);
        } else if (status & BATTLE_UNIT_STATUS_UNNAMED_0E) {
            battle_gfx_modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, -8, -8, -8, final_value);
        } else if ((status & BATTLE_UNIT_STATUS_UNDEAD) && unit->spritesheet_id != BATTLE_SPRITESHEET_ID_SKELETON
            && unit->spritesheet_id != BATTLE_SPRITESHEET_ID_GHOUL) {
            battle_gfx_modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, 4, 0, 5, final_value);
        } else {
            battle_gfx_tint_unit_palette_for_weather_and_tile(unit, tile_effect_level, final_value);
        }
        return;
    }
    battle_gfx_tint_unit_palette_for_weather_and_tile(unit, tile_effect_level, final_value);
}
