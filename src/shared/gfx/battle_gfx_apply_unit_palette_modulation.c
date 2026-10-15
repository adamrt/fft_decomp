#include "fft/battle_gfx.h"
#include "fft/battle_map.h"

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
