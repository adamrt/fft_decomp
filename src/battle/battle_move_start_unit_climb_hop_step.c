#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

/* Starts a unit's hop to a nearby tile (called from the knockback handler at
 * 0x8006cc94): derives the launch rise and flight frames from the landing
 * height, then sets the facing, destination tile and horizontal velocity for
 * the direction in bits 6-7 of `movement_value`.
 *
 * Case bodies store the destination before clearing the other velocity axis;
 * that order gives the target's map_x/step load order. */
void battle_move_start_unit_climb_hop_step(
    battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to) {
    s32 unused[4];
    battle_unit_height_data_t height_data;
    s32 rise;
    s32 frames;
    u8* step;
    s32 rise_height;
    s32 height;
    u32 animation;
    u32 word;

    step = &unit->movement_value;
    if (unit->current_unit_id_plus_one != 0
        && (battle_calculate_unit_height_data(&height_data, unit->current_unit_id_plus_one - 1),
            (height_data.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) == 0)) {
        height = height_data.total_height;
    } else {
        height = to->height * 2 + (to->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK) * unit->destination_edge_height;
    }
    rise_height = (height - -unit->screen.vy / 6) * 6 + 1;
    rise = SquareRoot12(rise_height * g_battle_move_jump_gravity * 2);
    unit->velocity.vy = -rise;
    frames = rise / g_battle_move_jump_gravity;
    switch (*step >> 6) {
    case 0:
        unit->centre_tile_offset = 0x16;
        unit->facing = 0xC00;
        unit->movement.bytes.destination_x = (u8)(unit->map_x + 1) + (*step & 3);
        unit->velocity.vz = 0;
        unit->velocity.vx = 0xE000 / frames;
        break;
    case 1:
        unit->centre_tile_offset = 0x1E;
        unit->facing = 0x400;
        unit->movement.bytes.destination_x = (u8)(unit->map_x - 1) - (*step & 3);
        unit->velocity.vz = 0;
        unit->velocity.vx = -0xE000 / frames;
        break;
    case 2:
        unit->centre_tile_offset = 0x12;
        unit->facing = 0;
        unit->movement.bytes.destination_y = (u8)(unit->map_y - 1) - (*step & 3);
        unit->velocity.vx = 0;
        unit->velocity.vz = -0xE000 / frames;
        break;
    case 3:
        /* Without this reorg moves this case's `li a0,0xe000` into the
         * `beq a1,3` delay slot, where the target keeps a nop; an asm insn
         * stops fill_slots_from_thread's scan (stop_search_p). */
        __asm__ volatile("");
        unit->centre_tile_offset = 0x1A;
        unit->facing = 0x800;
        unit->movement.bytes.destination_y = (u8)(unit->map_y + 1) + (*step & 3);
        unit->velocity.vx = 0;
        unit->velocity.vz = 0xE000 / frames;
        break;
    }
    animation = 0x30;
    if (unit->movement.word & BATTLE_MOTION_FLAG_ALTERNATE_SHORT_HOP) {
        animation = 0x31;
    }
    battle_unit_store_animation_facing(animation, (s16)unit->facing, unit);
    word = unit->movement.word;
    unit->movement.word = (word & ~BATTLE_MOTION_FLAG_ALTERNATE_SHORT_HOP) | ((((word >> 27) & 1) ^ 1) << 27);
    unit->movement.bytes.destination_z = (*step >> 5) & 1;
}
