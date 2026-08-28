#include "fft/battle.h"
#include "psx/types.h"

/* Start a unit's jump step toward the next path tile.
 *
 * The step byte's low two bits give the jump length in tiles and its top two
 * bits the direction. The vertical velocity scales with the length; a long
 * jump (length 2+) uses animation 0x1e with a per-direction facing, while a
 * short hop dismounts any rider and toggles movement bit 27 to alternate the
 * 0x30/0x31 hop animation. */
void battle_move_start_unit_jump_step(battle_unit_misc_data_t* unit) {
    u16 facings[4] = { 0xC00, 0x400, 0, 0x800 };
    u8 jump_offsets[4] = { 0x15, 0x1D, 0x11, 0x19 };
    u8 hop_offsets[4] = { 0x16, 0x1E, 0x12, 0x1A };
    u8* step;
    s32 animation;
    u32 word;
    s32 value;

    unit->velocity.vy = -g_battle_move_jump_gravity * ((((unit->movement_value & 3) * 0x1C000) + 0xE000) / 24576);
    step = &unit->movement_value;
    value = unit->movement_value;
    if ((value & 3) >= 2) {
        unit->centre_tile_offset = jump_offsets[(u8)value >> 6];
        battle_unit_store_animation_facing(0x1E, facings[unit->movement_value >> 6], unit);
    } else {
        unit->centre_tile_offset = hop_offsets[(u8)value >> 6];
        battle_unit_dismount_rider(unit);
        animation = 0x30;
        if (unit->movement.word & BATTLE_MOTION_FLAG_ALTERNATE_SHORT_HOP) {
            animation = 0x31;
        }
        battle_unit_store_animation_facing(animation, (s16)unit->facing, unit);
        word = unit->movement.word;
        unit->movement.word = (word & ~BATTLE_MOTION_FLAG_ALTERNATE_SHORT_HOP) | ((((word >> 27) & 1) ^ 1) << 27);
    }
    switch (*step >> 6) {
    case 0:
        unit->movement.bytes.destination_x = (u8)(unit->map_x + 1) + (*step & 3);
        unit->velocity.vx = 0x3000;
        unit->velocity.vz = 0;
        break;
    case 1:
        unit->movement.bytes.destination_x = (u8)(unit->map_x - 1) - (*step & 3);
        unit->velocity.vx = -0x3000;
        unit->velocity.vz = 0;
        break;
    case 2:
        unit->movement.bytes.destination_y = (u8)(unit->map_y - 1) - (*step & 3);
        unit->velocity.vx = 0;
        unit->velocity.vz = -0x3000;
        break;
    case 3:
        unit->movement.bytes.destination_y = (u8)(unit->map_y + 1) + (*step & 3);
        unit->velocity.vx = 0;
        unit->velocity.vz = 0x3000;
        break;
    }
    unit->movement.bytes.destination_z = (*step >> 5) & 1;
}
