#include "fft/battle.h"
#include "psx/types.h"

/* Start a unit's climb jump from its current tile onto the higher tile `to`.
 *
 * The rise velocity is sqrt(2 * gravity * height) for the height difference in
 * half-tile steps (6 screen units each) plus two half-steps per tile of jump
 * length; a second apex term extends the frame count. The step byte's top two
 * bits pick the direction, which sets the centre-tile offset, facing, the
 * destination coordinate, and the +-0xe000 horizontal velocity spread over the
 * frame count. `from` is unused. `unused` reproduces an unreferenced 24-byte
 * frame slot. The separate `height -=`/`height +=` statements keep the target's
 * in-place accumulation register; `rise_height` and `apex` keep the multiplication
 * order (term * 6 or 12 before gravity) that one folded expression reassociates.
 */
void battle_move_start_float_climb_jump_step(
    battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to) {
    s32 rise;
    s32 frames;
    u8* step;
    s32 unused[6];
    s32 rise_height;
    s32 height;
    s32 apex;

    step = &unit->movement_value;
    height = to->height * 2 + (to->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK) * unit->destination_edge_height + 2;
    height -= -unit->screen.vy / 6;
    height += ((unit->movement_value & 3) + 1) * 2;
    rise_height = height * 6;
    rise = SquareRoot12(rise_height * g_battle_move_jump_gravity * 2);
    frames = rise / g_battle_move_jump_gravity;
    unit->velocity.vy = -rise;
    apex = ((unit->movement_value & 3) + 1) * 12;
    frames += SquareRoot12(apex * g_battle_move_jump_gravity * 2) / g_battle_move_jump_gravity;
    switch (unit->movement_value >> 6) {
    case 0:
        unit->centre_tile_offset = 0x31;
        unit->facing = 0xC00;
        unit->velocity.vz = 0;
        unit->movement.bytes.destination_x = (u8)(unit->map_x + 1) + (unit->movement_value & 3);
        unit->velocity.vx = 0xE000 / frames;
        break;
    case 1:
        unit->centre_tile_offset = 0x39;
        unit->facing = 0x400;
        unit->velocity.vz = 0;
        unit->movement.bytes.destination_x = (u8)(unit->map_x - 1) - (unit->movement_value & 3);
        unit->velocity.vx = -0xE000 / frames;
        break;
    case 2:
        unit->centre_tile_offset = 0x2D;
        unit->facing = 0;
        unit->velocity.vx = 0;
        unit->movement.bytes.destination_y = (u8)(unit->map_y - 1) - (unit->movement_value & 3);
        unit->velocity.vz = -0xE000 / frames;
        break;
    case 3:
        unit->centre_tile_offset = 0x35;
        unit->facing = 0x800;
        unit->velocity.vx = 0;
        unit->movement.bytes.destination_y = (u8)(unit->map_y + 1) + (unit->movement_value & 3);
        unit->velocity.vz = 0xE000 / frames;
        break;
    }
    unit->movement.bytes.destination_z = (*step >> 5) & 1;
}
