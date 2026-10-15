/* Var, HasUnit, Gil and Date conditions with operand skips. */

#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_map_skip_3_operands(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 3;
}

void wldcore_opcode_map_skip_4_operands(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 4;
}

/* Reject the Var= condition when the script variable differs from the operand. */
void wldcore_opcode_map_check_var_equal(void) {
    s32 variable_id;
    s32 value;

    wldcore_script_read_operand_pair(&variable_id, &value);
    if (script_get_variable(variable_id) != value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

void wldcore_script_read_operand_pair(s32* out1, s32* out2) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 next_ip;

    *out1 = g_wldcore_script_state.data[*ip];
    next_ip = *ip + 1;
    *ip = next_ip;
    *out2 = g_wldcore_script_state.data[next_ip];
    *ip = *ip + 1;
}

/* Reject the Var>= condition when the script variable is below the operand. */
void wldcore_opcode_map_check_var_at_least(void) {
    s32 variable_id;
    s32 value;

    wldcore_script_read_operand_pair(&variable_id, &value);
    if (script_get_variable(variable_id) < value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Var<= condition when the script variable is above the operand. */
void wldcore_opcode_map_check_var_at_most(void) {
    s32 variable_id;
    s32 value;

    wldcore_script_read_operand_pair(&variable_id, &value);
    if (script_get_variable(variable_id) > value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the HasUnit condition when no roster unit has the operand's sprite set. */
void wldcore_opcode_map_check_roster_has_sprite_set(void) {
    s32 i;
    s32 sprite_set;
    u16* ipp;
    u16 ip;
    party_unit_t* unit;
    s32* flags;

    ipp = &g_wldcore_script_state.ip;
    ip = *ipp;
    sprite_set = g_wldcore_script_state.data[ip];
    *ipp = ip + 1;
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = wldcore_get_party_unit(i);
        if (unit->party_id != PARTY_ID_NONE) {
            if (unit->sprite_set == sprite_set) {
                break;
            }
        }
    }
    if (i == PARTY_ROSTER_SLOT_COUNT) {
        flags = &g_wldcore_script_state.flags;
        *flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

void wldcore_opcode_map_skip_1_operand_0x0c(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 1;
}

void wldcore_opcode_map_skip_1_operand_0x0d(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 1;
}

/* Reject the Gil>= condition when current war funds are below the operand. */
void wldcore_opcode_map_check_war_funds_at_least(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    u16 value = g_wldcore_script_state.data[idx];
    *ip = idx + 1;
    if (script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) < value) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Gil<= condition when current war funds are above the operand. */
void wldcore_opcode_map_check_war_funds_at_most(void) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 idx = *ip;
    s32 value = g_wldcore_script_state.data[idx];
    *ip = idx + 1;
    if (value < script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS)) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Date>= condition when the current date precedes the operand. */
void wldcore_opcode_map_check_date_on_or_after(void) {
    s32 month, day, current_month, current_day;
    s32* flags;

    wldcore_script_read_operand_pair_and_date(&month, &day, &current_month, &current_day);
    if ((current_month < month) || ((current_month == month) && (current_day < day))) {
        flags = &g_wldcore_script_state.flags;
        *flags = *flags | WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}
