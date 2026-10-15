#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Pause every map texture animation (their modes are saved and cleared) until
 * map_unfreeze. Ability effects and the event script use the pair. */
void map_freeze(void) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}

/* Resume the map texture animations paused by map_freeze. */
void map_unfreeze(void) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}

/* Event ColorField: tint or fade all 256 colors of the map CLUT row with
 * map_modify_palette. `first` is its color mode (map_color_mode_e) and
 * `second` its amount: 0 applies the colors at once, another value fades to
 * them. */
void map_color_field(s32 first, s32 second, s16 red, s16 green, s16 blue) {
    map_modify_palette(first, second, MAP_CLUT_ROW_MAP, 0, 1, red, green, blue);
}

/* Tints one unit's two CLUTs through map_modify_palette; `preset_color` is
 * the MAP_COLOR_MODE_* command and `time` its amount.
 *
 * The unit's CLUT of MAP_CLUT_ROW_UNITS takes the offsets as given. Its CLUT of
 * MAP_CLUT_ROW_UNIT_SHADOWS takes the same offsets while the unit stands on a
 * tile with depth, at or below the tile's surface, and the negated offsets
 * otherwise. An absent unit is skipped. */
void battle_gfx_apply_unit_palette_modulation(
    s32 preset_color, s32 time, s32 runtime_id, s16 red, s16 green, s16 blue) {
    SVECTOR screen_coordinates;
    battle_unit_t* unit;
    map_tile_t* tile;

    unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit == 0) {
        return;
    }

    battle_unit_copy_draw_coords(&screen_coordinates, unit);
    tile = map_get_tile_data_pointer(
        screen_coordinates.vx / MAP_TILE_SIZE, screen_coordinates.vz / MAP_TILE_SIZE, unit->map_z);

    map_modify_palette(preset_color, time, MAP_CLUT_ROW_UNITS, runtime_id, 0, red, green, blue);

    if (tile->depth_half_height.depth != 0
        && unit->world_coords.vy >= -(tile->height + tile->depth_half_height.depth) * 12) {
        map_modify_palette(preset_color, time, MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0, red, green, blue);
    } else {
        map_modify_palette(preset_color, time, MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0, -red, -green, -blue);
    }
}

/* battle_gfx_apply_unit_palette_modulation for one unit, or for all sixteen
 * when runtime_id is 16 or more. */
void battle_gfx_apply_unit_palette_modulation_to_units(
    s32 preset_color, s32 time, s32 runtime_id, s16 red, s16 green, s16 blue) {
    s32 index;

    if (runtime_id >= BATTLE_UNIT_RUNTIME_SLOT_COUNT) {
        for (index = 0; index < BATTLE_UNIT_RUNTIME_SLOT_COUNT; index++) {
            battle_gfx_apply_unit_palette_modulation(preset_color, time, index, red, green, blue);
        }
    } else {
        battle_gfx_apply_unit_palette_modulation(preset_color, time, runtime_id, red, green, blue);
    }
}
