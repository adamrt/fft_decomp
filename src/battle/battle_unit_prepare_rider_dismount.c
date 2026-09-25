#include "fft/battle.h"

/* Word view of misc +0x9c: the path count byte and the first packed path
 * step. The step layout matches the movement_path comment in battle.h
 * (direction in bits 6-7, 0x20 higher elevation, 0x10 path flag, jump length
 * in bits 0-1). Accessed as u32 bit-fields because the target reads and
 * writes the whole word. */
typedef struct battle_unit_rider_path_head {
    u32 count : 8;
    u32 jump_length : 2;
    u32 step_bit_2 : 1;
    u32 step_bit_3 : 1;
    u32 path_flag : 1;
    u32 higher_elevation : 1;
    u32 direction : 2;
    u32 _unknown_10 : 16;
} battle_unit_rider_path_head_t;

/* Word view of misc +0x118; the target clears the packed byte at +0x11a
 * (bits 16-23) with a word read-modify-write. */
typedef struct battle_unit_rider_move_state {
    u32 _unknown_00 : 16;
    u32 bit_0 : 1;
    u32 bits_1_3 : 3;
    u32 bit_4 : 1;
    u32 bits_5_7 : 3;
    u32 _unknown_18 : 8;
} battle_unit_rider_move_state_t;

/* Prepare a unit's one-step movement path and dismount it from its rider.
 *
 * Returns 1, or 0 after raising the pointer exception when the misc record is
 * missing. The 0x11a bit-fields must be cleared before the path step is built:
 * that order fixes the target's constant-load schedule. */
s32 battle_unit_prepare_rider_dismount(u32 misc_id, s32 direction, s32 jump_length) {
    battle_unit_misc_data_t* unit;
    battle_unit_rider_path_head_t* head;
    battle_unit_rider_move_state_t* state;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit != 0) {
        head = (battle_unit_rider_path_head_t*)&unit->movement_path_count;
        state = (battle_unit_rider_move_state_t*)&unit->movement_path[0x7b];
        unit->movement_path_count = 1;
        unit->walk_speed.word = 0x2000;
        unit->step_speed = 0x2000;
        unit->mount_byte = 0;
        unit->movement_path_offset = 0;
        state->bits_5_7 = 0;
        state->bit_4 = 0;
        state->bits_1_3 = 0;
        state->bit_0 = 0;
        head->jump_length = jump_length;
        head->step_bit_2 = 0;
        head->step_bit_3 = 0;
        head->path_flag = 0;
        head->higher_elevation = unit->map_z;
        head->direction = direction;
        unit->movement_flags = 0;
        battle_unit_dismount_rider_and_update_display(unit);
    } else {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return 1;
}
