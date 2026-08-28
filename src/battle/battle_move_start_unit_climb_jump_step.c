#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

/* Start a unit's jump across one or more tiles onto the destination tile.
 *
 * The landing height is the top of a unit standing on the destination (per
 * battle_calculate_unit_height_data) or, when the tile is empty or that unit
 * is flagged 0x80, the tile height plus its slope scaled by destination_edge_height. The
 * vertical speed and frame count come from the rise over the unit's current
 * screen height plus a clearance of two half-heights per tile of jump length.
 * The step byte's top two bits give the direction, as in
 * battle_move_start_unit_jump_step. `unused_10` reproduces an unreferenced
 * 16-byte frame slot below the height data. */
void battle_move_start_unit_climb_jump_step(
    battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to) {
    VECTOR unused_10;
    battle_unit_height_data_t height_data;
    u8* step;
    s32 height;
    s32 speed;
    s32 time;

    step = &unit->movement_value;
    if (unit->current_unit_id_plus_one != 0
        && (battle_calculate_unit_height_data(&height_data, unit->current_unit_id_plus_one - 1),
            (height_data.unit_flags & 0x80) == 0)) {
        height = height_data.total_height;
    } else {
        height = to->height * 2 + (to->depth_half_height & 0x1f) * unit->destination_edge_height + 2;
    }
    /* Grouping each constant with the gravity term keeps the target's order
     * (scale, then multiply by gravity); the flat spelling scales gravity first. */
    speed
        = SquareRoot12((height - -unit->screen.vy / 6 + ((*step & 3) + 1) * 2) * (6 * g_battle_move_jump_gravity) * 2);
    time = speed / g_battle_move_jump_gravity;
    unit->velocity.vy = -speed;
    time += SquareRoot12(((*step & 3) + 1) * (12 * g_battle_move_jump_gravity) * 2) / g_battle_move_jump_gravity;
    switch (*step >> 6) {
    case 0:
        unit->centre_tile_offset = 0x15;
        battle_unit_store_animation_facing(0x1e, 0xc00, unit);
        unit->movement.bytes.destination_x = (u8)(unit->map_x + 1) + (*step & 3);
        unit->velocity.vx = 0xe000 / time;
        unit->velocity.vz = 0;
        break;
    case 1:
        unit->centre_tile_offset = 0x1d;
        battle_unit_store_animation_facing(0x1e, 0x400, unit);
        unit->movement.bytes.destination_x = (u8)(unit->map_x - 1) - (*step & 3);
        unit->velocity.vx = -0xe000 / time;
        unit->velocity.vz = 0;
        break;
    case 2:
        unit->centre_tile_offset = 0x11;
        battle_unit_store_animation_facing(0x1e, 0, unit);
        unit->movement.bytes.destination_y = (u8)(unit->map_y - 1) - (*step & 3);
        unit->velocity.vz = -0xe000 / time;
        unit->velocity.vx = 0;
        break;
    case 3:
        unit->centre_tile_offset = 0x19;
        battle_unit_store_animation_facing(0x1e, 0x800, unit);
        unit->movement.bytes.destination_y = (u8)(unit->map_y + 1) + (*step & 3);
        unit->velocity.vz = 0xe000 / time;
        unit->velocity.vx = 0;
        break;
    }
    unit->movement.bytes.destination_z = (*step >> 5) & 1;
}
