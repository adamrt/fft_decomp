#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

SVECTOR* battle_unit_get_event_offset_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit == 0) {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return &unit->event_offset;
}

/*
 * Move a display unit to the supplied tile and facing.
 *
 * Clears the effect vectors, sets both the current and destination tile, and
 * recomputes the integer and fixed-point world positions from the tile.
 */
s32 battle_unit_place_in_new_location(u32 runtime_id, map_tile_coordinates_t* tile, s32 facing) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xFFFF);
    if (unit != 0) {
        unit->event_offset.vz = 0;
        unit->event_offset.vy = 0;
        unit->event_offset.vx = 0;
        unit->seq_offset.vz = 0;
        unit->seq_offset.vy = 0;
        unit->seq_offset.vx = 0;
        unit->screen_offset.vz = 0;
        unit->screen_offset.vy = 0;
        unit->screen_offset.vx = 0;
        unit->map_x = unit->movement.destination_x = tile->x;
        unit->map_y = unit->movement.destination_y = tile->y;
        unit->map_z = unit->movement.destination_z = tile->layer;
        unit->facing = facing << 10;
        unit->world_coords.vx = tile->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        unit->world_coords.vz = tile->y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        unit->world_coords.vy = map_calculate_slope_height(&unit->world_coords, unit->map_z);
        unit->real.vx = unit->world_coords.vx * ONE;
        unit->real.vz = unit->world_coords.vz * ONE;
        unit->real.vy = unit->world_coords.vy * ONE;
        if (unit->stats != 0) {
            battle_unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing);
        }
        return 1;
    }
    return 0;
}

s32 battle_unit_has_runtime_id(u32 runtime_id) {
    return battle_unit_get_by_runtime_id(runtime_id & 0xffff) != 0;
}

/* Sets BATTLE_MOTION_FLAG_SUPPRESS_FACING_UPDATE on a unit and clears its 16-bit sprite
 * display flags. */
void battle_unit_set_facing_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->sprite_display_flags = 0;
        unit->movement.suppress_facing_update = 1;
    }
}

void battle_unit_clear_facing_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->movement.suppress_facing_update = 0;
    }
}

void battle_unit_set_horizontal_flip_flag(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->horizontal_flip_flag = BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP;
    }
}

void battle_unit_clear_horizontal_flip_flag(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->horizontal_flip_flag = 0;
    }
}

void battle_unit_set_palette_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 1;
    }
}

void battle_unit_clear_palette_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 0;
    }
}

/* Reloads a unit's palette with its weather and time-of-day tint and no tile
 * effect level, for the ResetPalette event instruction. An absent unit is
 * skipped. */
void battle_gfx_apply_weather_time_tile_palette_mod_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        battle_gfx_tint_unit_palette_for_weather_and_tile(unit, 0, 1);
    }
}
