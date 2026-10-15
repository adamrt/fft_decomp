#include "fft/main.h"
#include "fft/script.h"

/* Walk the scenario's condition chains and report whether one selects an event.
 *
 * g_main_scenario_condition_offsets holds up to ten byte offsets into
 * g_main_scenario_condition_data, one per chain; a zero offset after the first
 * ends the list. A chain is a run of records, each a 16-bit type followed by
 * g_script_condition_param_lengths[type] 16-bit operands. Records are
 * evaluated while they pass and the first failure drops the chain; the RUN_EVENT
 * record that closes a chain stores its event id in the current-event variable,
 * and the function returns 1. It returns 0 when no chain gets that far.
 *
 * The checker always receives the five 16-bit values at the record, whatever the
 * type's length. Called at EventEnd and by the scenario-check command of
 * the scene's menu system-function dispatcher. */
s32 script_run_scenario_conditions(void) {
    s16* condition;
    s32 i;
    s32 shifted_offset;
    s32 offset;
    s32 result;

    for (i = 0; i < 10; i++) {
        /* The entry is a 16-bit byte offset into the script area; the
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
            result
                = script_check_scenario_condition(condition[0], condition[1], condition[2], condition[3], condition[4]);
            condition += g_script_condition_param_lengths[condition[0]] + 1;
        } while (result == EVENT_SCENARIO_CONDITION_RESULT_PASS);
        if (result == EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT) {
            return 1;
        }
    }
    return 0;
}
