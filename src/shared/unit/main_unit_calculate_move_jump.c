#include "fft/main.h"
#include "psx/types.h"

/* Add the Move +1/+2/+3 and Jump +1/+2/+3 abilities to Move and Jump; Move is
 * capped at 0xFC and Jump at 7. With keep_position 0, HP and MP are refilled
 * first. */
void main_unit_calculate_move_jump(unit_stats_t* unit, s32 keep_position) {
    u16 move;
    u16 jump;
    u8 flags;

    if (keep_position == 0) {
        unit->hp = unit->max_hp;
        unit->mp = unit->max_mp;
    }
    move = unit->move;
    flags = unit->ability_flags.movement[0];
    jump = unit->jump;
    if (flags & BATTLE_MOVEMENT_SET_1_MOVE_PLUS_1) {
        move += 1;
    }
    if (flags & BATTLE_MOVEMENT_SET_1_MOVE_PLUS_2) {
        move += 2;
    }
    if (flags & BATTLE_MOVEMENT_SET_1_MOVE_PLUS_3) {
        move += 3;
    }
    if (flags & BATTLE_MOVEMENT_SET_1_JUMP_PLUS_1) {
        jump += 1;
    }
    if (flags & BATTLE_MOVEMENT_SET_1_JUMP_PLUS_2) {
        jump += 2;
    }
    if (flags & BATTLE_MOVEMENT_SET_1_JUMP_PLUS_3) {
        jump += 3;
    }
    if (move >= 0xFD) {
        move = 0xFC;
    }
    if (jump >= 8) {
        jump = 7;
    }
    unit->move = move;
    unit->jump = jump;
}
