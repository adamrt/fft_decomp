#include "fft/wldcore.h"
#include "psx/types.h"

/* World-map script conditional set: walks the list of condition scripts for a
 * location, runs each through the opcode dispatcher until it reports a result,
 * and returns 1 for the first script that both succeeded (result bit 0x01) and
 * carries one of the caller's requested action flags.
 *
 * The block at 0x800d4644 is addressed as one record rather than as four
 * separate externs: target/wldcore.yaml names three of its members
 * independently -- g_wldcore_script_flags (+0x00), g_wldcore_script_data_ptr
 * (+0x08) and g_wldcore_script_ip (+0x0c) -- and the target materialises the
 * +0x04 address once and reaches +0x08 and +0x0c as `addiu` displacements off
 * it, which only the record view reproduces.
 *
 * Two operand-order devices:
 *
 *  - `base - (-offset)` at the two script-address computations. A plain `+`
 *    (in either order, or split into locals) canonicalises the masked
 *    halfword operand into `rs`, giving `addu v0,v0,v1` where the target has
 *    `addu v1,v1,v0`. `a - (-b)` reaches fold as MINUS over NEGATE and is
 *    rewritten to a PLUS without the commutative re-canonicalisation, so the
 *    base stays in `rs`, as in the wldcore_opcode_branch_if_* handlers. The
 *    first sum also seeds the v0/v1 roles for the whole function.
 *
 *  - The `flags` temp for the two result tests keeps the target's
 *    `and v0,v1,s4` operand order: the loaded flags word in `rs`, the caller's
 *    `action_mask` in `rt`. Reading the global directly at both tests
 *    reverses it (as for the `or rd,flags,mask` temp in
 *    main_smd_force_channel_func).
 *
 * The do/while condition deliberately reads the global rather than the temp:
 * the target reloads it there, after the dispatched call. */

s32 wldcore_script_process_conditional_set(s32 location, s32 action_mask) {
    s32 flags;
    u16 i;
    u16 entry;
    s32 offset;

    offset = g_wldcore_script_base[location] & 0xFFFE;
    g_wldcore_script_state.list = (u16*)((s32)g_wldcore_script_base - (-offset));
    for (i = 0; (entry = g_wldcore_script_state.list[i]) != 0; i++) {
        g_wldcore_script_state.data = (u16*)((s32)g_wldcore_script_base - (-(entry & 0xFFFE)));
        g_wldcore_script_state.ip = 0;
        g_wldcore_script_state.flags = 0;
        do {
            u16 ip = g_wldcore_script_state.ip;
            u16 op = g_wldcore_script_state.data[ip];
            g_wldcore_script_state.ip = ip + 1;
            g_wldcore_script_opcode_table[op]();
        } while ((g_wldcore_script_state.flags & 3) == 0);
        flags = g_wldcore_script_state.flags;
        if (flags & 1) {
            if (flags & action_mask) {
                return 1;
            }
        }
    }
    return 0;
}
