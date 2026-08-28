#include "fft/wldcore.h"
#include "psx/types.h"

/* The target's sum is `addu v1,v0,v1`: rs is the
 * g_wldcore_active_saved_record.counter word, rt the sign-extended
 * counter_delta halfword. Any `+` spelling canonicalises the halfword into rs.
 * `a - (-b)` reaches fold as MINUS_EXPR(a, NEGATE_EXPR(b)) and is rewritten to
 * a PLUS_EXPR without the commutative re-canonicalisation, which keeps the
 * target's operand order. */
void wldcore_opcode_branch_if_greater(void) {
    s32 value;
    s32 operand;
    u32 word;
    u16 flags;

    value = g_wldcore_active_saved_record.counter - (-g_wldcore_active_saved_record.counter_delta);
    if (0xffff < value) {
        value = 0xffff;
    }
    if (value < 0) {
        value = 0;
    }
    word = g_wldcore_opcode_state.instruction.word;
    operand = (word >> 8) & 0xffff;
    if (operand < value) {
        flags = g_wldcore_active_saved_record.state_flags;
        g_wldcore_active_saved_record.section = word >> 24;
        g_wldcore_active_saved_record.state_flags = flags | 2;
    } else {
        g_wldcore_active_saved_record.state_flags |= 4;
    }
}
