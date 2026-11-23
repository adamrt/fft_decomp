/* Resolve loaded spritesheets and update unit palettes, transparency and height. */

#include "fft/animation.h"
#include "fft/map.h"
#include "fft/map_palette.h"
#include "fft/map_unit.h"
#include "fft/script_variable.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/weather.h"
#include "psx/types.h"

/* Request `animation` with facing `facing` (0x400 per quarter turn) for the next animation update;
 * the request is dropped when that sequence is already playing in the same view. The request
 * is stored as the animation plus one, so 0 means none. */
void animation_store_with_facing(u32 animation, s32 facing, map_unit_t* unit) {
    animation++;
    unit->requested_animation = animation;
    unit->facing = facing;
}

/* animation_store_with_facing with the force flag: the animation
 * starts on the next update, from its beginning even when it is playing. */
void animation_force_with_facing(s32 animation, s32 facing, map_unit_t* unit) {
    animation++;
    unit->requested_animation = animation;
    unit->facing = facing;
    unit->movement.force_animation_update = 1;
}

/* The SHP frame tables of a sheet: one of the five resident ones (TYPE1 through OTHER), or the
 * battle's boss SHP for the higher SHP types. */
sprite_unit_shp_frame_tables_t* sprite_sheet_get_unit_shp(u32 spritesheet_id) {
    s32 value = g_sprite_sheet_data[spritesheet_id & 0xffff].shp_id;

    if (value < 5) {
        return &g_sprite_unit_shp_frame_tables[value];
    }
    return &g_sprite_boss_shp_frame_tables;
}

/* The SEQ table of a sheet: one of the five resident ones, or the table of
 * the battle's RUKA, ARUTE or KANZEN file. */
sprite_unit_seq_table_t* sprite_sheet_get_unit_seq(u32 spritesheet_id) {
    s32 value = g_sprite_sheet_data[spritesheet_id & 0xffff].seq_id;

    if (value < SPRITE_SEQ_RUKA) {
        return &g_sprite_unit_seq_tables[value].header;
    }
    return &g_sprite_boss_seq_tables.header;
}

/* The weapon SHP of a sheet. The weapon tables are 0xc40-byte records, a
 * sprite_weapon_shp_t with 0x200 frames and then its 0x100 sequence
 * pointers: WEP1 for SHP type TYPE1 and WEP2 for TYPE2. The other SHP types
 * index past the two records; none of their sequences starts a weapon block. */
sprite_weapon_shp_t* sprite_sheet_get_weapon_shp(u32 spritesheet_id) {
    return &g_sprite_weapon_graphics[g_sprite_sheet_data[spritesheet_id & 0xffff].shp_id].shp;
}

/* The weapon SEQ table of a sheet: the second part of the record that
 * sprite_sheet_get_weapon_shp describes. */
u8** sprite_sheet_get_weapon_seq(u32 spritesheet_id) {
    return g_sprite_weapon_graphics[g_sprite_sheet_data[spritesheet_id & 0xffff].shp_id].seq;
}

/* Every sheet shares the one effect SHP, EFF1. */
sprite_weapon_shp_t* sprite_sheet_get_effect_shp(u32 unused_spritesheet_id) {
    return &g_sprite_effect_graphics[0].shp;
}

/* Every sheet shares the one effect SEQ table, EFF1. */
u8** sprite_sheet_get_effect_seq(u32 unused_spritesheet_id) {
    return g_sprite_effect_graphics[0].seq;
}

/* Nonzero for a sheet that idles with the flying animations. */
u32 sprite_sheet_get_flying_flag(u32 spritesheet_id) {
    return g_sprite_sheet_data[spritesheet_id & 0xffff].flying_flag;
}

/* Center the unit on its map tile: its move destination becomes its tile, its world position the
 * tile center at its standing height, and its real position follows. */
void map_unit_set_move_and_world_coords(map_unit_t* unit) {
    unit->movement.destination_x = unit->map_x;
    unit->movement.destination_y = unit->map_y;
    unit->movement.destination_layer = unit->map_layer;
    unit->world_coords.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->world_coords.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->world_coords.vy = map_unit_calculate_height_from_world_coords(unit);
    map_unit_set_real_coords_from_world_coords(unit);
}

/* Moves a rider killed on its mount (pending_attack_result set) off it: the pair
 * is unlinked, the unit takes the relocation tile in dismount as its map position
 * and screen position, its battle stats follow, and the pending result is
 * cleared. */
void map_unit_dismount_dead_rider(map_unit_t* unit) {
    /* pending_attack_result is -1 when the action killed a rider on its mount. */
    if (unit->pending_attack_result != 0) {
        map_unit_rider_dismount_and_update_display(unit);
        unit->map_x = unit->dismount.x;
        unit->map_y = unit->dismount.y;
        unit->map_layer = unit->dismount.layer;
        map_unit_set_move_and_world_coords(unit);
        battle_unit_set_tile_position(unit->battle_unit->battle_unit_id, unit->map_x, unit->map_y, unit->map_layer,
            (unit->facing / UNIT_FACING_QUARTER_TURN));
        unit->pending_attack_result = 0;
    }
}

/* Return 1 when one of the nine VRAM spritesheet slots already holds `spritesheet_id`. */
s32 sprite_sheet_is_loaded(s16 spritesheet_id) {
    s32 slot;

    for (slot = 0; slot < SPRITE_SHEET_SLOT_COUNT; slot++) {
        if (g_sprite_sheet_slots[slot].spritesheet_id == spritesheet_id) {
            return 1;
        }
    }
    return 0;
}

/* Claim a VRAM spritesheet slot for `spritesheet_id`: reuse the slot that
 * already holds it, otherwise take the first free one. Returns the slot, or
 * reports SYSTEM_ERROR_NO_SPRITESHEET_SLOT and returns 0xFFFF when all
 * nine are taken. A slot's in_use holds the id of the sheet claiming it. */
s32 sprite_sheet_claim_slot(s16 spritesheet_id) {
    s32 slot;
    u16 found = 0xFF;

    for (slot = 0; slot < SPRITE_SHEET_SLOT_COUNT; slot++) {
        if (g_sprite_sheet_slots[slot].spritesheet_id == spritesheet_id) {
            g_sprite_sheet_slots[slot].in_use = spritesheet_id;
            found = slot;
            break;
        }
    }
    if (found == 0xFF) {
        for (slot = 0; slot < SPRITE_SHEET_SLOT_COUNT; slot++) {
            if (g_sprite_sheet_slots[slot].in_use == 0) {
                g_sprite_sheet_slots[slot].in_use = spritesheet_id;
                g_sprite_sheet_slots[slot].spritesheet_id = spritesheet_id;
                found = slot;
                break;
            }
        }
    }
    if (slot >= SPRITE_SHEET_SLOT_COUNT) {
        system_handle_error(SYSTEM_ERROR_NO_SPRITESHEET_SLOT);
        return 0xFFFF;
    }
    return found;
}

/* Reload a unit's palette tinted for its tile's shading, the weather and the
 * time of day.
 *
 * The second CLUT row is installed first from the slot's upper eight
 * palettes. The RGB offset starts at -4, -8 or -12 for the three tile shading
 * levels. Unless the map ignores weather, rain and storms cool it (less red,
 * more blue), night cools it further, and a unit more than 0x30 above the
 * ground at its position (in the air, as during Jump) stays untinted. Crystal,
 * chicken, frog and treasure sheets tint fixed RAM palettes instead of the
 * slot's.
 *
 * Each branch makes its own palette call, as the original evidently did:
 * cross-jumping merges them only after register allocation, so the extra
 * references still rank clut_index above unit in global allocation. The two snow
 * (weather bit 0) branches are identical. */
void sprite_palette_tint_unit(map_unit_t* unit, s32 level, s32 update_current_colors) {
    s32 clut_index;
    s32 stored_palette;
    s32 slot = unit->spritesheet_vram_slot;
    s32 weather;
    s32 blue;
    s32 green;
    s32 red;
    s16 standing_world_y;

    stored_palette = unit->stored_palette;
    clut_index = unit->map_unit_id;

    /* The second CLUT row comes from the upper eight palettes: the unit's own for job and
     * monster sheets, the first one for the rest. */
    if (g_sprite_sheet_slots[slot].spritesheet_id >= SPRITE_SHEET_ID_JOB_FIRST
        && g_sprite_sheet_slots[slot].spritesheet_id <= SPRITE_SHEET_ID_STEEL_GIANT) {
        map_palette_load_data(
            &g_sprite_sheet_slots[slot].clut[stored_palette + 8], MAP_CLUT_ROW_UNITS_SECOND, clut_index, 0);
    } else {
        map_palette_load_data(&g_sprite_sheet_slots[slot].clut[8], MAP_CLUT_ROW_UNITS_SECOND, clut_index, 0);
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
        const s32* weather_flags = &g_weather_flags;

        if (!(*weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER)) {
            weather = script_variable_get(EVENT_SCRIPT_VAR_WEATHER);
            /* Weather cools the tint (less red, more blue); at night the shift is stronger and
             * applies in clear weather too. The snow branches match the plain ones. */
            if (script_variable_get(EVENT_SCRIPT_VAR_TIME_OF_DAY) == TIME_OF_DAY_DAY) {
                if (*weather_flags & MAP_WEATHER_FLAG_SNOW) {
                    switch (weather) {
                    case WEATHER_NONE:
                    case WEATHER_NONE_ALT:
                        break;
                    case WEATHER_RAIN:
                        red -= 1;
                        blue += 1;
                        break;
                    case WEATHER_STORM:
                    case WEATHER_STRONG_STORM:
                    default:
                        red -= 2;
                        green -= 1;
                        blue += 2;
                        break;
                    }
                } else {
                    switch (weather) {
                    case WEATHER_NONE:
                    case WEATHER_NONE_ALT:
                        break;
                    case WEATHER_RAIN:
                        red -= 1;
                        blue += 1;
                        break;
                    case WEATHER_STORM:
                    case WEATHER_STRONG_STORM:
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
                    case WEATHER_NONE:
                    case WEATHER_NONE_ALT:
                        red -= 3;
                        green -= 1;
                        blue += 2;
                        break;
                    case WEATHER_RAIN:
                        red -= 4;
                        green -= 1;
                        blue += 3;
                        break;
                    case WEATHER_STORM:
                    case WEATHER_STRONG_STORM:
                    default:
                        red -= 4;
                        green -= 2;
                        blue += 4;
                        break;
                    }
                } else {
                    switch (weather) {
                    case WEATHER_NONE:
                    case WEATHER_NONE_ALT:
                        red -= 3;
                        green -= 1;
                        blue += 2;
                        break;
                    case WEATHER_RAIN:
                        red -= 4;
                        green -= 1;
                        blue += 3;
                        break;
                    case WEATHER_STORM:
                    case WEATHER_STRONG_STORM:
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

    /* y grows downward: a unit more than 0x30 above the ground at its position stays untinted. */
    standing_world_y = map_unit_calculate_height_from_world_coords(unit);
    if (!(g_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) && standing_world_y - 0x30 > unit->world_coords.vy) {
        blue = 0;
        green = 0;
        red = 0;
    }

    /* Crystal, chicken, frog and treasure sheets tint fixed RAM palettes instead of the slot's. */
    switch (unit->spritesheet_id) {
    case SPRITE_SHEET_ID_CRYSTAL:
        map_palette_set_from_other(g_sprite_other_palettes.crystal[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_CHICKEN:
        map_palette_set_from_other(g_sprite_other_palettes.chicken[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_FROG:
        map_palette_set_from_other(g_sprite_other_palettes.frog[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0, red,
            green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_TREASURE:
        map_palette_set_from_other(g_sprite_other_palettes.treasure[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, update_current_colors);
        break;
    default:
        map_palette_set_from_other(g_sprite_sheet_slots[slot].clut[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, update_current_colors);
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
static void modify_unit_palette(map_unit_t* unit, s32 mode, s32 clut_row, s32 clut_index, s32 all_colors, s16 red,
    s16 green, s16 blue, s32 update_current_colors) {
    s32 slot;
    s32 sheet;
    s32 palette;

    slot = unit->spritesheet_vram_slot;
    sheet = unit->spritesheet_id;
    palette = unit->stored_palette;
    switch (sheet) {
    case SPRITE_SHEET_ID_CRYSTAL:
        map_palette_modify_from_other(mode, g_sprite_other_palettes.crystal[palette], clut_row, clut_index, all_colors,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_CHICKEN:
        map_palette_modify_from_other(mode, g_sprite_other_palettes.chicken[palette], clut_row, clut_index, all_colors,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_FROG:
        map_palette_modify_from_other(mode, g_sprite_other_palettes.frog[palette], clut_row, clut_index, all_colors,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_TREASURE:
        map_palette_modify_from_other(mode, g_sprite_other_palettes.treasure[palette], clut_row, clut_index, all_colors,
            red, green, blue, update_current_colors);
        break;
    default:
        map_palette_modify_from_other(mode, g_sprite_sheet_slots[slot].clut[palette], clut_row, clut_index, all_colors,
            red, green, blue, update_current_colors);
        break;
    }
}

/* Tint a unit's palette for its first palette-affecting status.
 *
 * Unless the unit already has a palette modifier, tints its CLUT for the
 * first matching renderer status (mode/RGB pairs below; Undead skips Skeleton
 * and Ghoul spritesheets); otherwise, or with no tinting status, it falls back
 * to sprite_palette_tint_unit. */
static void update_unit_status_palette(map_unit_t* unit, s32 tile_effect_level, s32 update_current_colors) {
    u32 status;
    s32 unit_id;

    if (unit->fade_out_phase != UNIT_FADE_OUT_NONE) {
        return;
    }
    status = unit->status_flags_1_4;
    unit_id = unit->map_unit_id;
    if (status & UNIT_STATUS_PALETTE_MOD_MASK) {
        if (status & UNIT_STATUS_PETRIFY) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_GRAY, MAP_CLUT_ROW_UNITS, unit_id, 0, 0, 0, 0, update_current_colors);
        } else if (status & UNIT_STATUS_BLOOD_SUCK) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_GRAY, MAP_CLUT_ROW_UNITS, unit_id, 0, 3, -1, 8, update_current_colors);
        } else if (status & UNIT_STATUS_BERSERK) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE, MAP_CLUT_ROW_UNITS, unit_id, 0, 8, 0, 0, update_current_colors);
        } else if (status & UNIT_STATUS_POISON) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, 0, 8, 0, update_current_colors);
        } else if (status & UNIT_STATUS_REGEN) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE, MAP_CLUT_ROW_UNITS, unit_id, 0, 0, 0, 8, update_current_colors);
        } else if (status & UNIT_STATUS_OIL) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, -4, -4, -4, update_current_colors);
        } else if (status & UNIT_STATUS_UNNAMED_0E) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, -8, -8, -8, update_current_colors);
        } else if ((status & UNIT_STATUS_UNDEAD) && unit->spritesheet_id != SPRITE_SHEET_ID_SKELETON
            && unit->spritesheet_id != SPRITE_SHEET_ID_GHOUL) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, 4, 0, 5, update_current_colors);
        } else {
            sprite_palette_tint_unit(unit, tile_effect_level, update_current_colors);
        }
        return;
    }
    sprite_palette_tint_unit(unit, tile_effect_level, update_current_colors);
}

/* Refresh the status palette of a unit and of its mount partner, unless suppressed. */
void sprite_palette_update(map_unit_t* unit, s32 tile_effect_level, s32 update_current_colors) {
    map_unit_t* partner;

    if (!(unit->movement.suppress_palette_update)) {
        update_unit_status_palette(unit, tile_effect_level, update_current_colors);
    }
    if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
        partner = map_unit_get(unit->mount_partner_map_unit_id);
        if (partner != 0 && !(partner->movement.suppress_palette_update)) {
            update_unit_status_palette(partner, tile_effect_level, update_current_colors);
        }
    }
}

/* Enables the status bubble, restarting its timer, for a unit with a status that
 * shows a bubble (UNIT_STATUS_BUBBLE_MASK) and for the acting unit;
 * disables it for any other unit. */
void sprite_status_bubble_update_active(map_unit_t* unit) {
    if ((unit->status_flags_1_4 & UNIT_STATUS_BUBBLE_MASK) != 0 || unit->map_unit_id == g_unit_source_map_unit_id) {
        unit->status_bubble_active = 1;
        unit->status_bubble_timer = 0;
    } else {
        unit->status_bubble_active = 0;
    }
}

/* Draws a unit with the Transparent status semi-transparent with averaging blend
 * (the blend bits cleared), and any other unit opaque. */
void sprite_update_transparency(map_unit_t* unit) {
    if ((unit->status_flags_5_6 & UNIT_STATUS_TRANSPARENT) != 0) {
        unit->sprite_display_flags
            = (unit->sprite_display_flags & ~SPRITE_DISPLAY_BLEND_MASK) | SPRITE_DISPLAY_SEMI_TRANSPARENT;
    } else {
        unit->sprite_display_flags = unit->sprite_display_flags & ~SPRITE_DISPLAY_SEMI_TRANSPARENT;
    }
}

/* Mirror the Float status into movement.floating and, unless the unit is in the air for Jump
 * (Jump, its height flag, or a Jump being removed), set it to its standing height and refresh its
 * real position. */
void map_unit_update_float_height(map_unit_t* unit) {
    if (unit->status_flags_5_6 & UNIT_STATUS_FLOAT) {
        unit->movement.floating = 1;
    } else {
        unit->movement.floating = 0;
    }
    if ((unit->status_flags_5_6 & (UNIT_STATUS_JUMP | UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) == 0
        && (unit->statuses_to_remove_5_6 & UNIT_STATUS_JUMP) == 0) {
        unit->world_coords.vy = map_unit_calculate_height_from_world_coords(unit);
        map_unit_set_real_coords_from_world_coords(unit);
    }
}
