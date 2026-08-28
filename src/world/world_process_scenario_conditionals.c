#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * The BATTLE sibling at 0x801425b0 is instruction-for-instruction equivalent
 * apart from its data addresses.
 *
 * The signed halving is written out because `v / 2` over a sign-extended
 * halfword cannot reach the target's shape. The target keeps `lhu` plus an
 * explicit `sll 16` / `sra 16` pair and reads the sign bit off the *shifted*
 * value (`srl 31` on the `sll` result, not on the extended one), which is only
 * possible while the `sll` result has two users. Spell the halving as `v / 2`
 * and the shift chain becomes single-use, combine folds `lhu`+`sll`+`sra` into
 * one `lh`, and the function comes out 208 bytes. Naming the shifted value
 * gives the second user back. It also keeps the `i != 0` test on the extended
 * value: `shorten_compare` would strip a narrowing cast compared against a
 * constant, which is what sinks the extension below both branches.
 */

extern u16 g_main_scenario_condition_offsets[];    /* 10 script byte offsets */
extern s16 g_main_scenario_condition_data[];       /* condition script words */
extern u8 g_world_scenario_condition_arg_counts[]; /* instruction length by opcode */
s32 world_process_scenario_conditionals(void) {
    s16* condition;
    s32 i;
    s32 shifted;
    s32 offset;
    s32 result;

    for (i = 0; i < 10; i++) {
        shifted = g_main_scenario_condition_offsets[i] << 16;
        offset = shifted >> 16;
        if (i != 0 && offset == 0) {
            return 0;
        }
        /* offset / 2: the halved word index into the script area. */
        condition = &g_main_scenario_condition_data[(offset + (s32)((u32)shifted >> 31)) >> 1];
        do {
            result = world_script_check_scenario_condition(
                condition[0], condition[1], condition[2], condition[3], condition[4]);
            condition += g_world_scenario_condition_arg_counts[condition[0]] + 1;
        } while (result == 1);
        if (result == 2) {
            return 1;
        }
    }
    return 0;
}
