#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

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
        } while ((g_wldcore_script_state.flags
                     & (WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_RESULT_CONDITION_FAILED))
            == 0);
        flags = g_wldcore_script_state.flags;
        if (flags & WLDCORE_SCRIPT_RESULT_ACTION_REACHED) {
            if (flags & action_mask) {
                return 1;
            }
        }
    }
    return 0;
}

void wldcore_opcode_map_nop(void) {
}

void wldcore_opcode_map_skip_1_operand(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 1;
}

void wldcore_opcode_map_skip_2_operands(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 2;
}

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

void wldcore_opcode_map_request_text(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_TEXT;
}

/* Stage the three operands for a deep-dungeon entry request.
 *
 * Each operand advances the script IP before the request becomes ready.
 */
void wldcore_opcode_map_request_deep_dungeon_entry(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[2] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_DEEP_DUNGEON_ENTRY;
}

/* Stage the two operands for the deferred SetVar2 request.
 *
 * This opcode only prepares the request; a later dispatcher performs it.
 */
void wldcore_opcode_map_request_set_var_2(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_SET_VAR_2;
}

/* Stage the two operands for a deferred DrawPath request.
 *
 * The request flag is published after both operands and IP updates.
 */
void wldcore_opcode_map_request_draw_path(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_DRAW_PATH;
}

void wldcore_opcode_map_request_erase_path(void) {
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.args[1] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
    g_wldcore_script_state.ip++;
    g_wldcore_script_state.flags |= WLDCORE_SCRIPT_REQUEST_ERASE_PATH;
}

/* Stage the location ID for a deferred DrawLocation request. */
void wldcore_opcode_map_request_draw_location(void) {
    g_wldcore_script_flags |= WLDCORE_SCRIPT_REQUEST_DRAW_LOCATION;
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
}

/* Stage the location ID for a deferred EraseLocation request. */
void wldcore_opcode_map_request_erase_location(void) {
    g_wldcore_script_flags |= WLDCORE_SCRIPT_REQUEST_ERASE_LOCATION;
    g_wldcore_script_state.args[0] = g_wldcore_script_state.data[g_wldcore_script_state.ip];
}

/* Resolve the script-selected location and focus the map on its window entry.
 */
void wldcore_opcode_map_focus_location(void) {
    u16 ip = g_wldcore_script_state.ip;
    u16* data = g_wldcore_script_state.data;
    u16 entry;
    s32 index;

    entry = data[ip];
    g_wldcore_script_state.args[0] = entry;
    if (entry == 0xff) {
        index = g_wldcore_map_projection_state.marker.kind;
        g_wldcore_proposition_selected_entry = index + 1;
    } else {
        index = entry;
        g_wldcore_proposition_selected_entry = index + 1;
    }
    if (wldcore_map_is_point_outside_projection_bounds(&g_wldcore_map_dots[index].render.position) != 0) {
        s32* flags = &g_wldcore_script_state.flags;

        *flags |= WLDCORE_SCRIPT_ACTION_FOCUS_LOCATION;
    }
    {
        s32* flags = &g_wldcore_script_state.flags;

        *flags |= WLDCORE_SCRIPT_RESULT_ACTION_REACHED;
    }
}

void wldcore_opcode_map_check_ramza_brave_at_least(void) {
    party_unit_t* party;
    u16 ip;
    s32 val;

    party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (party->bravery < val) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Roster index (0..15) of the first occupied slot holding one of Ramza's
 * identities; 16 when there is none. */
s32 wldcore_get_ramza_roster_index(void) {
    s32 i;
    for (i = 0; i < PARTY_MEMBER_SLOT_COUNT; i++) {
        party_unit_t* party = wldcore_get_party_unit(i);
        if (party->party_id != PARTY_ID_NONE && party->sprite_set != CHARACTER_IDENTITY_ENTD_NONE
            && party->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
            return i;
        }
    }
    return i;
}

/* Reject the MCBrave<= condition when Ramza's Brave is above the operand. */
void wldcore_opcode_map_check_ramza_brave_at_most(void) {
    party_unit_t* party;
    u16 ip;
    s32 val;

    party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (val < party->bravery) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the MCFaith>= condition when Ramza's Faith is below the operand. */
void wldcore_opcode_map_check_ramza_faith_at_least(void) {
    party_unit_t* party;
    u16 ip;
    s32 val;

    party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (party->faith < val) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the MCFaith<= condition when Ramza's Faith is above the operand. */
void wldcore_opcode_map_check_ramza_faith_at_most(void) {
    party_unit_t* party;
    u16 ip;
    s32 val;

    party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (val < party->faith) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}
