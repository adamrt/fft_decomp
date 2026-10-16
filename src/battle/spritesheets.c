/* Post-action text animation, spritesheet SHP and SEQ lookups, spritesheet slot claims, unit palette tints and status
 * bubble triggers. */

#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/main_gfx.h"
#include "fft/map.h"
#include "fft/script.h"
#include "psx/libc.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Animate and queue the three numeric post-action display sprites.
 *
 * Scales each digit sprite by its per-frame zoom curve, starts the next
 * queued result once the animation passes frame 50, and retires the display
 * after frame 61.
 *
 * Keep the curves external: local initializers change the following switch
 * table's alignment relative to the original rodata. */
void battle_gfx_animate_post_action_text(battle_unit_t* unit, const SVECTOR* screen_coords) {
    SVECTOR position;
    SVECTOR zoom_0 = g_battle_status_bubble_zoom;
    SVECTOR zoom_1 = g_battle_status_bubble_zoom;
    SVECTOR zoom_2 = g_battle_status_bubble_zoom;
    s16 scale_0[21];
    s16 scale_1[21];
    s16 scale_2[21];
    s16 critical_scale_0[21];
    s16 critical_scale_1[21];
    s16 critical_scale_2[21];
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;
    action_result_display_t* display;
    u16 progress;

    memcpy(scale_0, g_battle_numeric_digit_0_zoom_curve, sizeof(scale_0));
    memcpy(scale_1, g_battle_numeric_digit_1_zoom_curve, sizeof(scale_1));
    memcpy(scale_2, g_battle_numeric_digit_2_zoom_curve, sizeof(scale_2));
    memcpy(critical_scale_0, g_battle_numeric_digit_0_critical_zoom_curve, sizeof(critical_scale_0));
    memcpy(critical_scale_1, g_battle_numeric_digit_1_critical_zoom_curve, sizeof(critical_scale_1));
    memcpy(critical_scale_2, g_battle_numeric_digit_2_critical_zoom_curve, sizeof(critical_scale_2));
    display_0 = unit->numeric_display.displays[0];
    display_1 = unit->numeric_display.displays[1];
    display_2 = unit->numeric_display.displays[2];
    progress = unit->numeric_display.progress + g_main_system_animation_speed;
    display = &unit->numeric_display;
    if (progress < 21) {
        if (unit->numeric_display.selector & BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL) {
            main_util_set_svector(
                &zoom_0, critical_scale_0[progress], critical_scale_0[progress], critical_scale_0[progress]);
            main_util_set_svector(
                &zoom_1, critical_scale_1[progress], critical_scale_1[progress], critical_scale_1[progress]);
            main_util_set_svector(
                &zoom_2, critical_scale_2[progress], critical_scale_2[progress], critical_scale_2[progress]);
        } else {
            switch (unit->numeric_display.selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
            case BATTLE_NUMERIC_DISPLAY_HP_DAMAGE ... BATTLE_NUMERIC_DISPLAY_MA_GAIN:
                main_util_set_svector(&zoom_0, scale_0[progress], scale_0[progress], scale_0[progress]);
                main_util_set_svector(&zoom_1, scale_1[progress], scale_1[progress], scale_1[progress]);
                main_util_set_svector(&zoom_2, scale_2[progress], scale_2[progress], scale_2[progress]);
                break;
            case BATTLE_NUMERIC_DISPLAY_MISSED:
            case BATTLE_NUMERIC_DISPLAY_GUARDED:
            case BATTLE_NUMERIC_DISPLAY_GAINED_EXP:
            case BATTLE_NUMERIC_DISPLAY_GAINED_JP:
                main_util_set_svector(&zoom_0, scale_1[progress], scale_1[progress], scale_1[progress]);
                main_util_set_svector(&zoom_1, scale_1[progress], scale_1[progress], scale_1[progress]);
                main_util_set_svector(&zoom_2, scale_1[progress], scale_1[progress], scale_1[progress]);
                break;
            }
        }
    } else if (progress >= 50 && unit->numeric_display.progress < 50) {
        if (battle_gfx_build_next_action_result_display(unit) != 0) {
            return;
        }
        if (battle_gfx_build_next_special_action_result_display(unit) != 0) {
            return;
        }
        unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
            = unit->numeric_display.displays[2]->tpage = 0x3f;
        battle_map_modify_palette(
            MAP_COLOR_MODE_BASE, 2, MAP_CLUT_ROW_ACTION_RESULTS, unit->runtime_id, 0, -31, -31, -31);
        battle_gfx_set_sprite_part_semi_transparent(display_0, 0, 1);
        battle_gfx_set_sprite_part_semi_transparent(display_1, 0, 1);
        battle_gfx_set_sprite_part_semi_transparent(display_2, 0, 1);
    } else if (progress >= 61) {
        if (battle_gfx_build_next_action_result_display(unit) != 0) {
            return;
        }
        if (battle_gfx_build_next_special_action_result_display(unit) != 0) {
            return;
        }
        unit->numeric_display.active = 0;
    }
    position.vx = screen_coords->vx + 0x15;
    position.vy = screen_coords->vy - 8;
    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case BATTLE_NUMERIC_DISPLAY_HP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_CT_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_PA_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MA_DAMAGE:
        position.vy = screen_coords->vy - 0x30;
        break;
    case BATTLE_NUMERIC_DISPLAY_HP_HEALING:
    case BATTLE_NUMERIC_DISPLAY_MP_HEALING:
    case BATTLE_NUMERIC_DISPLAY_SPEED_GAIN:
    case BATTLE_NUMERIC_DISPLAY_CT_GAIN:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_GAIN:
    case BATTLE_NUMERIC_DISPLAY_FAITH_GAIN:
    case BATTLE_NUMERIC_DISPLAY_PA_GAIN:
    case BATTLE_NUMERIC_DISPLAY_MA_GAIN:
        position.vy = screen_coords->vy - 0x10;
        break;
    case BATTLE_NUMERIC_DISPLAY_MISSED:
    case BATTLE_NUMERIC_DISPLAY_GUARDED:
    case BATTLE_NUMERIC_DISPLAY_GAINED_EXP:
    case BATTLE_NUMERIC_DISPLAY_GAINED_JP:
        break;
    }
    battle_gfx_construct_polygon_data_for_units(
        display_0, display_0->part_count, 0, &position, 0, 0, &zoom_0, main_gfx_get_otag() + 1);
    battle_gfx_construct_polygon_data_for_units(
        display_1, display_1->part_count, 0, &position, 0, 0, &zoom_1, main_gfx_get_otag() + 1);
    battle_gfx_construct_polygon_data_for_units(
        display_2, display_2->part_count, 0, &position, 0, 0, &zoom_2, main_gfx_get_otag() + 1);
    display->progress += g_main_system_animation_speed;
}

/* Request `animation` with facing `facing` (0x400 per quarter turn) for the next animation update;
 * the request is dropped when that sequence is already playing in the same view. */
void battle_unit_store_animation_facing(u32 animation, s32 facing, battle_unit_t* unit) {
    animation++;
    unit->requested_animation = animation;
    unit->facing = facing;
}

/* battle_unit_store_animation_facing with the force flag: the animation
 * starts on the next update, from its beginning even when it is playing. */
void battle_unit_store_animation_facing_movement_data(s32 animation, s32 facing, battle_unit_t* unit) {
    animation++;
    unit->requested_animation = animation;
    unit->facing = facing;
    unit->movement.force_animation_update = 1;
}

/* The SHP frame tables of a sheet: one of the five resident ones (TYPE1 through OTHER), or the
 * battle's boss SHP for the higher SHP types. */
battle_gfx_unit_shp_frame_tables_t* battle_gfx_get_unit_shp_by_spritesheet_id(u32 spritesheet_id) {
    s32 value = g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].shp_id;

    if (value < 5) {
        return &g_battle_gfx_unit_shp_frame_tables[value];
    }
    return &g_battle_gfx_boss_shp_frame_tables;
}

/* The SEQ table of a sheet: one of the five resident ones, or the table of
 * the battle's RUKA, ARUTE or KANZEN file. */
battle_gfx_unit_seq_table_t* battle_gfx_get_unit_seq_by_spritesheet_id(u32 spritesheet_id) {
    s32 value = g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].seq_id;

    if (value < BATTLE_SEQ_RUKA) {
        return &g_battle_gfx_unit_seq_tables[value].header;
    }
    return &g_battle_gfx_boss_seq_tables.header;
}

/* The weapon SHP of a sheet. The weapon tables are 0xc40-byte records, a
 * battle_gfx_weapon_shp_t with 0x200 frames and then its 0x100 sequence
 * pointers: WEP1 for SHP type TYPE1 and WEP2 for TYPE2. The other SHP types
 * index past the two records; none of their sequences starts a weapon block. */
battle_gfx_weapon_shp_t* battle_gfx_get_weapon_shp_by_spritesheet_id(u32 spritesheet_id) {
    return &g_battle_gfx_weapon_graphics[g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].shp_id].shp;
}

/* The weapon SEQ table of a sheet: the second part of the record that
 * battle_gfx_get_weapon_shp_by_spritesheet_id describes. */
u8** battle_gfx_get_weapon_seq_by_spritesheet_id(u32 spritesheet_id) {
    return g_battle_gfx_weapon_graphics[g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].shp_id].seq;
}

/* Every sheet shares the one effect SHP, EFF1. */
battle_gfx_weapon_shp_t* battle_gfx_get_effect_shp_data(u32 unused_spritesheet_id) {
    return &g_battle_gfx_effect_graphics[0].shp;
}

/* Every sheet shares the one effect SEQ table, EFF1. */
u8** battle_gfx_get_effect_seq_data(u32 unused_spritesheet_id) {
    return g_battle_gfx_effect_graphics[0].seq;
}

/* Nonzero for a sheet that idles with the flying animations. */
u32 battle_gfx_get_spritesheet_flying_flag(u32 spritesheet_id) {
    return g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].flying_flag;
}

/* Center the unit on its map tile: its move destination becomes its tile, its world position the
 * tile center at its standing height, and its real position follows. */
void battle_unit_set_move_and_world_coords(battle_unit_t* unit) {
    unit->movement.destination_x = unit->map_x;
    unit->movement.destination_y = unit->map_y;
    unit->movement.destination_z = unit->map_z;
    unit->world_coords.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->world_coords.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->world_coords.vy = battle_gfx_calculate_world_height_from_world_coords(unit);
    battle_unit_set_real_coords_from_world_coords(unit);
}

/* Moves a rider killed on its mount (pending_attack_result set) off it: the pair
 * is unlinked, the unit takes the relocation tile in dismount as its map position
 * and screen position, its battle stats follow, and the pending result is
 * cleared. */
void battle_unit_set_map_coords_after_death_dismount(battle_unit_t* unit) {
    /* pending_attack_result is -1 when the action killed a rider on its mount. */
    if (unit->pending_attack_result != 0) {
        battle_unit_dismount_rider_and_update_display(unit);
        unit->map_x = unit->dismount.x;
        unit->map_y = unit->dismount.y;
        unit->map_z = unit->dismount.layer;
        battle_unit_set_move_and_world_coords(unit);
        battle_unit_set_tile_position(
            unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, (unit->facing / BATTLE_FACING_QUARTER_TURN));
        unit->pending_attack_result = 0;
    }
}

/* Return 1 when one of the nine VRAM spritesheet slots already holds `spritesheet_id`. */
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
    u16 found = 0xFF;

    for (slot = 0; slot < 9; slot++) {
        if (g_battle_gfx_spritesheet_slots[slot].spritesheet_id == spritesheet_id) {
            g_battle_gfx_spritesheet_slots[slot].in_use = spritesheet_id;
            found = slot;
            break;
        }
    }
    if (found == 0xFF) {
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
    return found;
}

/* Reload a unit's palette tinted for tile effects, weather and time of day.
 *
 * The unit's spritesheet palette is installed first (monster sheets
 * 0x60-0x9a use the upper eight palettes). The RGB offset starts at -4, -8 or
 * -0xc for tile effect levels 1-3; unless weather bit 1 is set, script
 * variables 0x23 (weather) and 0x24 (night) shift it further, and a unit more
 * than 0x30 above the ground height at its position (in the air, as during
 * Jump) stays untinted. Crystal, chicken, frog and treasure sheets take fixed
 * palettes.
 *
 * Each branch makes its own palette call, as the original evidently did:
 * cross-jumping merges them only after register allocation, so the extra
 * references still rank clut_index above unit in global allocation. The two snow
 * (weather bit 0) branches are identical. */
void battle_gfx_tint_unit_palette_for_weather_and_tile(battle_unit_t* unit, s32 level, s32 final_value) {
    s32 clut_index;
    s32 stored_palette;
    s32 slot = unit->spritesheet_vram_slot;
    s32 weather;
    s32 blue;
    s32 green;
    s32 red;
    s16 screen_z;

    stored_palette = unit->stored_palette;
    clut_index = unit->runtime_id;

    if (g_battle_gfx_spritesheet_slots[slot].spritesheet_id >= BATTLE_SPRITESHEET_ID_JOB_FIRST
        && g_battle_gfx_spritesheet_slots[slot].spritesheet_id <= BATTLE_SPRITESHEET_ID_STEEL_GIANT) {
        battle_map_load_palette_data(
            &g_battle_gfx_spritesheet_slots[slot].clut[stored_palette + 8], MAP_CLUT_ROW_UNITS_SECOND, clut_index, 0);
    } else {
        battle_map_load_palette_data(
            &g_battle_gfx_spritesheet_slots[slot].clut[8], MAP_CLUT_ROW_UNITS_SECOND, clut_index, 0);
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
        const s32* weather_flags = &g_battle_map_weather_flags;

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
    if (!(g_battle_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) && screen_z - 0x30 > unit->world_coords.vy) {
        blue = 0;
        green = 0;
        red = 0;
    }

    switch (unit->spritesheet_id) {
    case BATTLE_SPRITESHEET_ID_CRYSTAL:
        battle_map_update_palette_from_other(g_battle_gfx_crystal_palettes[stored_palette], MAP_CLUT_ROW_UNITS,
            clut_index, 0, red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_CHICKEN:
        battle_map_update_palette_from_other(g_battle_gfx_other_palettes.chicken[stored_palette], MAP_CLUT_ROW_UNITS,
            clut_index, 0, red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_FROG:
        battle_map_update_palette_from_other(g_battle_gfx_frog_palettes[stored_palette], MAP_CLUT_ROW_UNITS, clut_index,
            0, red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_TREASURE:
        battle_map_update_palette_from_other(g_battle_gfx_treasure_palettes[stored_palette], MAP_CLUT_ROW_UNITS,
            clut_index, 0, red, green, blue, final_value);
        break;
    default:
        battle_map_update_palette_from_other(g_battle_gfx_spritesheet_slots[slot].clut[stored_palette],
            MAP_CLUT_ROW_UNITS, clut_index, 0, red, green, blue, final_value);
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
static void battle_gfx_modify_unit_palette(battle_unit_t* unit, s32 mode, s32 clut_row, s32 clut_index, s32 preset,
    s16 red, s16 green, s16 blue, s32 final_value) {
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
static void battle_gfx_update_unit_status_palette(battle_unit_t* unit, s32 tile_effect_level, s32 final_value) {
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

/* Mirror the Float status into movement.floating and, unless the unit is in the air for Jump
 * (Jump, its height flag, or a Jump being removed), set it to its standing height and refresh its
 * real position. */
void battle_unit_update_float_and_jump_height(battle_unit_t* unit) {
    if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_FLOAT) {
        unit->movement.floating = 1;
    } else {
        unit->movement.floating = 0;
    }
    if ((unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_JUMP | BATTLE_UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) == 0
        && (unit->statuses_to_remove_5_6 & BATTLE_UNIT_STATUS_JUMP) == 0) {
        unit->world_coords.vy = battle_gfx_calculate_world_height_from_world_coords(unit);
        battle_unit_set_real_coords_from_world_coords(unit);
    }
}
