#include "fft/battle.h"
#include "psx/types.h"

/*
 * Return the offset just past a jump-target marker whose id is target_id.
 *
 * A BackTarget search scans forward from the start of the block to
 * limit_offset and keeps the last match; an intervening 0xd9 instruction or a
 * ForwardTarget with the same id invalidates earlier matches. Other searches
 * scan forward from limit_offset for target_opcode or alternate_opcode (-1
 * disables it) until the End instruction. A missing target exits the current
 * thread.
 */
s32 battle_script_find_jump_target(
    s32 limit_offset, event_opcode_e target_opcode, s32 alternate_opcode, s32 target_id) {
    u8* base;
    s32 offset;
    s32 found;
    s32 opcode;

    found = -1;
    if (target_opcode == EVENT_OPCODE_BACK_TARGET) {
        for (offset = 0; offset < limit_offset; offset += 1 + g_battle_script_event_instruction_sizes[opcode]) {
            base = (u8*)g_battle_event_block;
            opcode = base[offset];
            if (opcode == EVENT_OPCODE_UNKNOWN_D9
                || (opcode == EVENT_OPCODE_FORWARD_TARGET && (base + offset)[1] == target_id)) {
                found = -1;
            } else if (opcode == target_opcode && (base + offset)[1] == target_id) {
                found = offset;
            }
        }
        if (found >= 0) {
            return found + 2;
        }
    } else {
        while ((opcode = ((u8*)g_battle_event_block)[limit_offset]) != EVENT_OPCODE_END) {
            if (opcode == target_opcode && ((u8*)g_battle_event_block + limit_offset)[1] == target_id) {
                return limit_offset + 2;
            }
            if (alternate_opcode != -1 && opcode == alternate_opcode
                && ((u8*)g_battle_event_block + limit_offset)[1] == target_id) {
                return limit_offset + 2;
            }
            limit_offset += 1 + g_battle_script_event_instruction_sizes[opcode];
        }
    }
    /* Does not return; the target has no return value on this path. */
    battle_thread_exit_current();
}
