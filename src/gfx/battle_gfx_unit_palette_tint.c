/* VRAM slot claim and the weather/tile/status palette tints of units. */

#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/types.h"

s32 battle_gfx_is_spritesheet_id_loaded(s16 spritesheet_id) {
    s32 slot;

    for (slot = 0; slot < 9; slot++) {
        if (g_battle_gfx_spritesheet_slots[slot].spritesheet_id == spritesheet_id) {
            return 1;
        }
    }
    return 0;
}

/* Claim a VRAM spritesheet slot for `spritesheet_id`: reuse the slot that
 * already holds it, otherwise take the first free one, and report error 0xB when all nine are taken. */
s32 battle_gfx_claim_spritesheet_slot(s16 spritesheet_id) {
    s32 slot;
    s32 found;

    found = 0xFF;
    for (slot = 0; slot < 9; slot++) {
        if (g_battle_gfx_spritesheet_slots[slot].spritesheet_id == spritesheet_id) {
            g_battle_gfx_spritesheet_slots[slot].in_use = spritesheet_id;
            found = slot;
            break;
        }
    }
    if ((found & 0xFFFF) == 0xFF) {
        for (slot = 0; slot < 9; slot++) {
            if (g_battle_gfx_spritesheet_slots[slot].in_use == 0) {
                g_battle_gfx_spritesheet_slots[slot].in_use = spritesheet_id;
                g_battle_gfx_spritesheet_slots[slot].spritesheet_id = spritesheet_id;
                found = slot;
                break;
            }
        }
    }
    if (slot >= 9) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_NO_SPRITESHEET_SLOT);
        return 0xFFFF;
    }
    return found & 0xFFFF;
}

/* Reload a unit's palette tinted for tile effects, weather and time of day.
 *
 * The unit's spritesheet palette is installed first (monster sheets
 * 0x60-0x9a use the upper eight palettes). The RGB offset starts at -4, -8 or
 * -0xc for tile effect levels 1-3; unless weather bit 1 is set, script
 * variables 0x23 (weather) and 0x24 (night) shift it further, and a unit more
 * than 0x30 below its projected screen Z stays untinted. Crystal, chicken,
 * frog and treasure sheets take fixed palettes.
 *
 * Each branch makes its own palette call, as the original evidently did:
 * cross-jumping merges them only after register allocation, so the extra
 * references still rank clut_index above unit in global allocation. The two snow
 * (weather bit 0) branches are identical. */
void battle_gfx_tint_unit_palette_for_weather_and_tile(battle_unit_t* unit, s32 level, s32 final_value) {
    s32 clut_index;
    s32 stored_palette;
    s32 slot;
    s32 weather;
    s32 blue;
    s32 green;
    s32 red;
    s16 screen_z;

    slot = unit->spritesheet_vram_slot;
    stored_palette = unit->stored_palette;
    clut_index = unit->runtime_id;

    if (g_battle_gfx_spritesheet_slots[slot].spritesheet_id >= BATTLE_SPRITESHEET_ID_JOB_FIRST
        && g_battle_gfx_spritesheet_slots[slot].spritesheet_id <= BATTLE_SPRITESHEET_ID_STEEL_GIANT) {
        map_load_palette_data(
            &g_battle_gfx_spritesheet_slots[slot].clut[stored_palette + 8], MAP_CLUT_ROW_UNITS_SECOND, clut_index, 0);
    } else {
        map_load_palette_data(&g_battle_gfx_spritesheet_slots[slot].clut[8], MAP_CLUT_ROW_UNITS_SECOND, clut_index, 0);
    }

    blue = 0;
    green = 0;
    red = 0;
    switch (level) {
    case MAP_TILE_SHADING_DARK:
        blue = -4;
        green = -4;
        red = -4;
        break;
    case MAP_TILE_SHADING_DARKER:
        blue = -8;
        green = -8;
        red = -8;
        break;
    case MAP_TILE_SHADING_DARKEST:
        blue = -0xc;
        green = -0xc;
        red = -0xc;
        break;
    }

    {
        const s32* weather_flags = &g_map_weather_flags;

        if (!(*weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER)) {
            weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
            if (script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) == 0) {
                if (*weather_flags & MAP_WEATHER_FLAG_SNOW) {
                    switch (weather) {
                    case BATTLE_WEATHER_NONE:
                    case BATTLE_WEATHER_NONE_ALT:
                        break;
                    case BATTLE_WEATHER_RAIN:
                        red -= 1;
                        blue += 1;
                        break;
                    case BATTLE_WEATHER_STORM:
                    case BATTLE_WEATHER_STRONG_STORM:
                    default:
                        red -= 2;
                        green -= 1;
                        blue += 2;
                        break;
                    }
                } else {
                    switch (weather) {
                    case BATTLE_WEATHER_NONE:
                    case BATTLE_WEATHER_NONE_ALT:
                        break;
                    case BATTLE_WEATHER_RAIN:
                        red -= 1;
                        blue += 1;
                        break;
                    case BATTLE_WEATHER_STORM:
                    case BATTLE_WEATHER_STRONG_STORM:
                    default:
                        red -= 2;
                        green -= 1;
                        blue += 2;
                        break;
                    }
                }
            } else {
                if (*weather_flags & MAP_WEATHER_FLAG_SNOW) {
                    switch (weather) {
                    case BATTLE_WEATHER_NONE:
                    case BATTLE_WEATHER_NONE_ALT:
                        red -= 3;
                        green -= 1;
                        blue += 2;
                        break;
                    case BATTLE_WEATHER_RAIN:
                        red -= 4;
                        green -= 1;
                        blue += 3;
                        break;
                    case BATTLE_WEATHER_STORM:
                    case BATTLE_WEATHER_STRONG_STORM:
                    default:
                        red -= 4;
                        green -= 2;
                        blue += 4;
                        break;
                    }
                } else {
                    switch (weather) {
                    case BATTLE_WEATHER_NONE:
                    case BATTLE_WEATHER_NONE_ALT:
                        red -= 3;
                        green -= 1;
                        blue += 2;
                        break;
                    case BATTLE_WEATHER_RAIN:
                        red -= 4;
                        green -= 1;
                        blue += 3;
                        break;
                    case BATTLE_WEATHER_STORM:
                    case BATTLE_WEATHER_STRONG_STORM:
                    default:
                        red -= 4;
                        green -= 2;
                        blue += 4;
                        break;
                    }
                }
            }
        }
    }

    screen_z = battle_gfx_calculate_world_height_from_world_coords(unit);
    if (!(g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) && screen_z - 0x30 > unit->world_coords.vy) {
        blue = 0;
        green = 0;
        red = 0;
    }

    switch (unit->spritesheet_id) {
    case BATTLE_SPRITESHEET_ID_CRYSTAL:
        map_update_palette_from_other(g_battle_gfx_crystal_palettes[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_CHICKEN:
        map_update_palette_from_other(g_battle_gfx_other_palettes.chicken[stored_palette], MAP_CLUT_ROW_UNITS,
            clut_index, 0, red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_FROG:
        map_update_palette_from_other(g_battle_gfx_frog_palettes[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_TREASURE:
        map_update_palette_from_other(g_battle_gfx_treasure_palettes[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, final_value);
        break;
    default:
        map_update_palette_from_other(g_battle_gfx_spritesheet_slots[slot].clut[stored_palette], MAP_CLUT_ROW_UNITS,
            clut_index, 0, red, green, blue, final_value);
        break;
    }
}

/* Apply palette modulation to a renderer-side unit's current CLUT.
 *
 * Crystal, Chicken, Frog, and Treasure use their dedicated RAM palette
 * tables; every other sheet uses its VRAM-slot palette. The color parameters
 * must remain s16 to preserve the target's register allocation and tail
 * sharing.
 */
void battle_gfx_modify_unit_palette(battle_unit_t* unit, s32 mode, s32 clut_row, s32 clut_index, s32 preset, s16 red,
    s16 green, s16 blue, s32 final_value) {
    s32 slot;
    s32 sheet;
    s32 palette;

    slot = unit->spritesheet_vram_slot;
    sheet = unit->spritesheet_id;
    palette = unit->stored_palette;
    switch (sheet) {
    case BATTLE_SPRITESHEET_ID_CRYSTAL:
        battle_gfx_modify_vram_palette(
            mode, g_battle_gfx_crystal_palettes[palette], clut_row, clut_index, preset, red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_CHICKEN:
        battle_gfx_modify_vram_palette(mode, g_battle_gfx_other_palettes.chicken[palette], clut_row, clut_index, preset,
            red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_FROG:
        battle_gfx_modify_vram_palette(
            mode, g_battle_gfx_frog_palettes[palette], clut_row, clut_index, preset, red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_TREASURE:
        battle_gfx_modify_vram_palette(
            mode, g_battle_gfx_treasure_palettes[palette], clut_row, clut_index, preset, red, green, blue, final_value);
        break;
    default:
        battle_gfx_modify_vram_palette(mode, g_battle_gfx_spritesheet_slots[slot].clut[palette], clut_row, clut_index,
            preset, red, green, blue, final_value);
        break;
    }
}

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

/* Enables the status bubble, restarting its timer, for a unit with a status that
 * shows a bubble (BATTLE_UNIT_STATUS_BUBBLE_MASK) and for the acting unit;
 * disables it for any other unit. */
void battle_gfx_update_status_bubble_graphic_trigger(battle_unit_t* unit) {
    if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_BUBBLE_MASK) != 0
        || unit->runtime_id == g_battle_unit_source_runtime_id) {
        unit->status_bubble_active = 1;
        unit->status_bubble_timer = 0;
    } else {
        unit->status_bubble_active = 0;
    }
}

/* Draws a unit with the Transparent status semi-transparent with averaging blend
 * (the blend bits cleared), and any other unit opaque. */
void battle_gfx_update_sprite_transparency_flag(battle_unit_t* unit) {
    if ((unit->status_flags_5_6 & BATTLE_UNIT_STATUS_TRANSPARENT) != 0) {
        unit->sprite_display_flags
            = (unit->sprite_display_flags & ~BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
    } else {
        unit->sprite_display_flags = unit->sprite_display_flags & ~BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
    }
}
