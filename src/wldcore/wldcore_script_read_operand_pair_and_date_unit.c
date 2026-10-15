#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Read the next two script operands (a month and a day) and the calendar's
 * current month and day. */
void wldcore_script_read_operand_pair_and_date(
    s32* out_month, s32* out_day, s32* out_current_month, s32* out_current_day) {
    u16* ipp;
    u16 ip;

    ipp = &g_wldcore_script_state.ip;
    *out_month = g_wldcore_script_state.data[*ipp];
    ip = *ipp + 1;
    *ipp = ip;
    *out_day = g_wldcore_script_state.data[ip];
    *ipp = *ipp + 1;
    *out_current_month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    *out_current_day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
}

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

/* Reject the Casualties>= condition when the current count is below the operand. */
void wldcore_opcode_map_check_casualties_at_least(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    u16 value = g_wldcore_script_state.data[idx];
    *ip = idx + 1;
    if (script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES) < value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Casualties<= condition when the current count is above the operand. */
void wldcore_opcode_map_check_casualties_at_most(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    s32 value = g_wldcore_script_state.data[idx];
    *ip = idx + 1;
    if (value < script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES)) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Stage the two 32-bit fields of the event identifier for a deferred LoadEvent request. */
void wldcore_opcode_map_request_load_event(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_LOAD_EVENT;
}

/* Stage four variable/value pairs for a deferred Choice request. */
void wldcore_opcode_map_request_choice(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        g_wldcore_script_state.choice_vars[i] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
        g_wldcore_script_state.args[i] = g_wldcore_script_state.data[++g_wldcore_script_state.ip];
        g_wldcore_script_state.ip++;
    }
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_CHOICE;
}

/* World Map Script 0x1C - SetVar: read next two script args and set a variable. */
void wldcore_opcode_map_set_var(void) {
    s32 variable_id;
    s32 value;

    wldcore_script_read_operand_pair(&variable_id, &value);
    script_set_variable(variable_id, value);
}
