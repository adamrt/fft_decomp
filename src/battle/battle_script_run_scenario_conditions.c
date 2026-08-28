#include "fft/battle.h"
#include "psx/types.h"

s32 battle_script_run_scenario_conditions(void) {
    s16* condition;
    s32 i;
    s32 shifted_offset;
    s32 offset;
    s32 result;

    for (i = 0; i < 10; i++) {
        /* The entry is a halfword byte offset into the script area; the
         * target sign-extends it with a live `<< 16` value, taking the
         * division's sign bit from that value rather than from the
         * sign-extended one. Spelled as `offset = (s16)g_main_scenario_condition_offsets[i]` with
         * `offset / 2`, combine folds the shift pair into a single `lh` and the
         * function comes out three instructions short, so the shift pair
         * and the by-two division are spelled out here. */
        shifted_offset = g_main_scenario_condition_offsets[i] << 16;
        offset = shifted_offset >> 16;
        if (i != 0 && offset == 0) {
            return 0;
        }
        condition = &g_main_scenario_condition_data[(offset + (s32)((u32)shifted_offset >> 31)) >> 1];
        do {
            result = battle_script_check_scenario_condition(
                condition[0], condition[1], condition[2], condition[3], condition[4]);
            condition += g_battle_script_condition_param_lengths[condition[0]] + 1;
        } while (result == 1);
        if (result == 2) {
            return 1;
        }
    }
    return 0;
}
