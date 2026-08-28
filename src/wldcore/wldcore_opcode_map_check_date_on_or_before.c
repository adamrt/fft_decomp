#include "fft/wldcore.h"

/* Reject the Date<= condition when the current date follows the operand. */
void wldcore_opcode_map_check_date_on_or_before(void) {
    s32 month, day, current_month, current_day;
    s32* flags;

    wldcore_script_read_operand_pair_and_date(&month, &day, &current_month, &current_day);
    if ((current_month > month) || ((current_month == month) && (current_day > day))) {
        flags = &g_wldcore_script_state.flags;
        *flags = *flags | WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}
