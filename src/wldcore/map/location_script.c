/* Location scripts test story conditions and request world-map events, choices and visible routes. */

#include "fft/location.h"
#include "fft/party.h"
#include "fft/script_variable.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/world_map.h"
#include "psx/types.h"

static void location_script_opcode_00_nop(void);
static void location_script_opcode_14_skip_1_operand(void);
static void location_script_opcode_05_skip_2_operands(void);
static void location_script_opcode_17_skip_3_operands(void);
static void location_script_opcode_18_skip_4_operands(void);
static void location_script_opcode_01_check_var_equal(void);
static void location_script_opcode_02_check_var_at_least(void);
static void location_script_opcode_03_check_var_at_most(void);
static void location_script_opcode_04_check_roster_has_character(void);
static void location_script_opcode_0c_skip_1_operand(void);
static void location_script_opcode_0d_skip_1_operand(void);
static void location_script_opcode_0e_check_war_funds_at_least(void);
static void location_script_opcode_0f_check_war_funds_at_most(void);
static void location_script_opcode_10_check_date_on_or_after(void);
static void location_script_opcode_11_check_date_on_or_before(void);
static void location_script_opcode_12_check_casualties_at_least(void);
static void location_script_opcode_13_check_casualties_at_most(void);
static void location_script_opcode_19_request_load_event(void);
static void location_script_opcode_1a_request_choice(void);
static void location_script_opcode_1c_set_var(void);
static void location_script_opcode_1d_request_text(void);
static void location_script_opcode_1e_request_deep_dungeon_entry(void);
static void location_script_opcode_1f_request_set_var_2(void);
static void location_script_opcode_20_request_draw_path(void);
static void location_script_opcode_21_request_erase_path(void);
static void location_script_opcode_22_request_draw_location(void);
static void location_script_opcode_23_request_erase_location(void);
static void location_script_opcode_24_focus_location(void);
static void location_script_opcode_25_check_ramza_brave_at_least(void);
static void location_script_opcode_26_check_ramza_brave_at_most(void);
static void location_script_opcode_27_check_ramza_faith_at_least(void);
static void location_script_opcode_28_check_ramza_faith_at_most(void);

/* Scalar view of g_location_script_state.flags for the opcode handlers that
 * read-modify-write it: the member spelling there shares one base register
 * with the args/data accesses instead of the target's absolute addresses. */
static s32 g_location_script_flags;

static void operand_pair_read(s32* out1, s32* out2);
static void operand_dates_read(s32* out_month, s32* out_day, s32* out_current_month, s32* out_current_day);
static s32 ramza_slot_find(void);

/* Opcode-indexed dispatch, defined after its handlers below. */
static void (*g_location_script_opcodes[41])(void);

/* Run the map script blocks of `location`'s conditional set in order, each until a condition fails
 * or it stages an action; return 1 at the first staged action with a flag in `action_mask`, else
 * 0. */
s32 location_script_run(s32 location, s32 action_mask) {
    s32 flags;
    u16 i;
    u16 entry;
    s32 offset = g_location_script_base[location] & 0xFFFE;

    g_location_script_state.list = (u16*)((u8*)g_location_script_base + offset);
    for (i = 0; (entry = g_location_script_state.list[i]) != 0; i++) {
        g_location_script_state.data = (u16*)((u8*)g_location_script_base + (entry & 0xFFFE));
        g_location_script_state.ip = 0;
        g_location_script_state.flags = 0;
        do {
            u16 ip = g_location_script_state.ip;
            u16 op = g_location_script_state.data[ip];
            g_location_script_state.ip = ip + 1;
            g_location_script_opcodes[op]();
        } while ((g_location_script_state.flags
                     & (LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_RESULT_CONDITION_FAILED))
            == 0);
        flags = g_location_script_state.flags;
        if ((flags & LOCATION_SCRIPT_RESULT_ACTION_REACHED) && (flags & action_mask)) {
            return 1;
        }
    }
    return 0;
}

/* Map script opcodes 0x00 and 0x16: do nothing. */
static void location_script_opcode_00_nop(void) {
}

/* Map script opcodes 0x14 and 0x15: skip one operand without testing anything. */
static void location_script_opcode_14_skip_1_operand(void) {
    g_location_script_state.ip += 1;
}

/* Map script opcodes 0x05 to 0x0B and 0x1B: skip two operands without testing anything. */
static void location_script_opcode_05_skip_2_operands(void) {
    g_location_script_state.ip += 2;
}

/* Map script opcode 0x17: skip three operands without testing anything. */
static void location_script_opcode_17_skip_3_operands(void) {
    g_location_script_state.ip += 3;
}

/* Map script opcode 0x18: skip four operands without testing anything. */
static void location_script_opcode_18_skip_4_operands(void) {
    g_location_script_state.ip += 4;
}

/* Reject the Var= condition when the script variable differs from the operand. */
static void location_script_opcode_01_check_var_equal(void) {
    s32 variable_id;
    s32 value;

    operand_pair_read(&variable_id, &value);
    if (script_variable_get(variable_id) != value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Read the next two 16-bit map script operands and step past them. */
static void operand_pair_read(s32* out1, s32* out2) {
    u16* ip = &g_location_script_state.ip;
    u16 next_ip;

    *out1 = g_location_script_state.data[*ip];
    next_ip = *ip + 1;
    *ip = next_ip;
    *out2 = g_location_script_state.data[next_ip];
    *ip = *ip + 1;
}

/* Reject the Var>= condition when the script variable is below the operand. */
static void location_script_opcode_02_check_var_at_least(void) {
    s32 variable_id;
    s32 value;

    operand_pair_read(&variable_id, &value);
    if (script_variable_get(variable_id) < value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Var<= condition when the script variable is above the operand. */
static void location_script_opcode_03_check_var_at_most(void) {
    s32 variable_id;
    s32 value;

    operand_pair_read(&variable_id, &value);
    if (script_variable_get(variable_id) > value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject HasUnit when no occupied roster slot has the operand's character identity.
 * The native scripts test Beowulf, joined Mustadio and Dragon Reis here. */
static void location_script_opcode_04_check_roster_has_character(void) {
    s32 i;
    s32 character_identity;
    u16* ipp;
    u16 ip;
    party_unit_t* unit;

    ipp = &g_location_script_state.ip;
    ip = *ipp;
    character_identity = g_location_script_state.data[ip];
    *ipp = ip + 1;
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = party_roster_unit_get(i);
        if (unit->party_id != PARTY_ID_NONE) {
            if (unit->sprite_set == character_identity) {
                break;
            }
        }
    }
    if (i == PARTY_ROSTER_SLOT_COUNT) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Map script opcode 0x0C: skip one operand without testing anything (its own copy of the 0x14
 * body). */
static void location_script_opcode_0c_skip_1_operand(void) {
    g_location_script_state.ip += 1;
}

/* Map script opcode 0x0D: skip one operand without testing anything (its own copy of the 0x14
 * body). */
static void location_script_opcode_0d_skip_1_operand(void) {
    g_location_script_state.ip += 1;
}

/* Reject the Gil>= condition when current war funds are below the operand. */
static void location_script_opcode_0e_check_war_funds_at_least(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    u16 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS) < value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Gil<= condition when current war funds are above the operand. */
static void location_script_opcode_0f_check_war_funds_at_most(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    s32 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (value < script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS)) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Date>= condition when the current date precedes the operand. */
static void location_script_opcode_10_check_date_on_or_after(void) {
    s32 month, day, current_month, current_day;

    operand_dates_read(&month, &day, &current_month, &current_day);
    if ((current_month < month) || ((current_month == month) && (current_day < day))) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Read a month and day operand pair from the map script, as
 * operand_pair_read does, along with the current month and day. */
static void operand_dates_read(s32* out_month, s32* out_day, s32* out_current_month, s32* out_current_day) {
    u16* ipp;
    u16 ip;

    ipp = &g_location_script_state.ip;
    *out_month = g_location_script_state.data[*ipp];
    ip = *ipp + 1;
    *ipp = ip;
    *out_day = g_location_script_state.data[ip];
    *ipp = *ipp + 1;
    *out_current_month = script_variable_get(EVENT_SCRIPT_VAR_MONTH);
    *out_current_day = script_variable_get(EVENT_SCRIPT_VAR_DAY);
}

/* Reject the Date<= condition when the current date follows the operand. */
static void location_script_opcode_11_check_date_on_or_before(void) {
    s32 month, day, current_month, current_day;

    operand_dates_read(&month, &day, &current_month, &current_day);
    if ((current_month > month) || ((current_month == month) && (current_day > day))) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Casualties>= condition when the current count is below the operand. */
static void location_script_opcode_12_check_casualties_at_least(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    u16 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (script_variable_get(EVENT_SCRIPT_VAR_CASUALTIES) < value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Casualties<= condition when the current count is above the operand. */
static void location_script_opcode_13_check_casualties_at_most(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    s32 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (value < script_variable_get(EVENT_SCRIPT_VAR_CASUALTIES)) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Stage a deferred LoadEvent request: the event to play and its transition
 * mode (event_transition_open). */
static void location_script_opcode_19_request_load_event(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_LOAD_EVENT;
}

/* Stage four battle-map/event pairs for a deferred Choice request. */
static void location_script_opcode_1a_request_choice(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        g_location_script_state.choice_map_ids[i] = g_location_script_state.data[g_location_script_state.ip];
        g_location_script_state.args[i] = g_location_script_state.data[++g_location_script_state.ip];
        g_location_script_state.ip++;
    }
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_CHOICE;
}

/* Map script opcode 0x1C (SetVar): set the script variable named by the first
 * operand to the second. */
static void location_script_opcode_1c_set_var(void) {
    s32 variable_id;
    s32 value;

    operand_pair_read(&variable_id, &value);
    script_variable_set(variable_id, value);
}

/* Stage the text operand for a deferred Text request (opcode 0x1D). */
static void location_script_opcode_1d_request_text(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_TEXT;
}

/* Stage the three operands for a deep-dungeon entry request.
 *
 * Each operand advances the script IP before the request becomes ready.
 */
static void location_script_opcode_1e_request_deep_dungeon_entry(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[2] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_DEEP_DUNGEON_ENTRY;
}

/* Stage the two operands for the deferred SetVar2 request.
 *
 * This opcode only prepares the request; a later dispatcher performs it.
 */
static void location_script_opcode_1f_request_set_var_2(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_SET_VAR_2;
}

/* Stage the two operands for a deferred DrawPath request.
 *
 * The request flag is published after both operands and IP updates.
 */
static void location_script_opcode_20_request_draw_path(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_DRAW_PATH;
}

/* Stage the two operands for a deferred ErasePath request (opcode 0x21). */
static void location_script_opcode_21_request_erase_path(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_ERASE_PATH;
}

/* Map script opcode 0x22: stage the location ID for a deferred DrawLocation
 * request. Unlike other opcodes it leaves ip on its operand, which is
 * harmless because staging the request ends the block. */
static void location_script_opcode_22_request_draw_location(void) {
    g_location_script_flags |= LOCATION_SCRIPT_REQUEST_DRAW_LOCATION;
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
}

/* Map script opcode 0x23: stage the location ID for a deferred EraseLocation
 * request, leaving ip on its operand like DrawLocation. */
static void location_script_opcode_23_request_erase_location(void) {
    g_location_script_flags |= LOCATION_SCRIPT_REQUEST_ERASE_LOCATION;
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
}

/* Map script opcode 0x24 (FocusLocation): select the operand's location (0xff
 * for the current one) and, only when its dot lies outside the view, request
 * the focus-location level that moves the map to it. It always ends the block
 * and, like DrawLocation, leaves ip on its operand. */
static void location_script_opcode_24_focus_location(void) {
    u16 ip = g_location_script_state.ip;
    u16* data = g_location_script_state.data;
    u16 entry;
    s32 index;

    entry = data[ip];
    g_location_script_state.args[0] = entry;
    if (entry == 0xff) {
        index = g_world_map_marker_state.marker.location_id;
        g_location_selected = index + 1;
    } else {
        index = entry;
        g_location_selected = index + 1;
    }
    if (world_map_point_needs_pan(&g_world_map_dots[index].render.position) != 0) {
        g_location_script_state.flags |= LOCATION_SCRIPT_ACTION_FOCUS_LOCATION;
    }
    g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_ACTION_REACHED;
}

/* Reject the MCBrave>= condition when Ramza's Brave is below the operand. */
static void location_script_opcode_25_check_ramza_brave_at_least(void) {
    party_unit_t* party = party_roster_unit_get(ramza_slot_find());
    u16 ip;
    s32 val;

    ip = g_location_script_state.ip;
    val = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (party->brave < val) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Return the first party slot holding Ramza (any of his chapter sprite sets), or
 * PARTY_MEMBER_SLOT_COUNT when none does. Callers use the result unchecked, so
 * without Ramza the MCBrave and MCFaith checks read roster slot 16, the first
 * guest slot, instead of failing (retail quirk). */
static s32 ramza_slot_find(void) {
    s32 i;
    for (i = 0; i < PARTY_MEMBER_SLOT_COUNT; i++) {
        party_unit_t* party = party_roster_unit_get(i);
        if (party->party_id != PARTY_ID_NONE && party->sprite_set != CHARACTER_IDENTITY_ENTD_NONE
            && party->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
            return i;
        }
    }
    return i;
}

/* Reject the MCBrave<= condition when Ramza's Brave is above the operand. */
static void location_script_opcode_26_check_ramza_brave_at_most(void) {
    party_unit_t* party = party_roster_unit_get(ramza_slot_find());
    u16 ip;
    s32 val;

    ip = g_location_script_state.ip;
    val = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (val < party->brave) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the MCFaith>= condition when Ramza's Faith is below the operand. */
static void location_script_opcode_27_check_ramza_faith_at_least(void) {
    party_unit_t* party = party_roster_unit_get(ramza_slot_find());
    u16 ip;
    s32 val;

    ip = g_location_script_state.ip;
    val = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (party->faith < val) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the MCFaith<= condition when Ramza's Faith is above the operand. */
static void location_script_opcode_28_check_ramza_faith_at_most(void) {
    party_unit_t* party = party_roster_unit_get(ramza_slot_find());
    u16 ip;
    s32 val;

    ip = g_location_script_state.ip;
    val = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (val < party->faith) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Original opcode table; repeated entries retain their shared skip handlers. */
static void (*g_location_script_opcodes[41])(void) = {
    location_script_opcode_00_nop,
    location_script_opcode_01_check_var_equal,
    location_script_opcode_02_check_var_at_least,
    location_script_opcode_03_check_var_at_most,
    location_script_opcode_04_check_roster_has_character,
    location_script_opcode_05_skip_2_operands,
    location_script_opcode_05_skip_2_operands,
    location_script_opcode_05_skip_2_operands,
    location_script_opcode_05_skip_2_operands,
    location_script_opcode_05_skip_2_operands,
    location_script_opcode_05_skip_2_operands,
    location_script_opcode_05_skip_2_operands,
    location_script_opcode_0c_skip_1_operand,
    location_script_opcode_0d_skip_1_operand,
    location_script_opcode_0e_check_war_funds_at_least,
    location_script_opcode_0f_check_war_funds_at_most,
    location_script_opcode_10_check_date_on_or_after,
    location_script_opcode_11_check_date_on_or_before,
    location_script_opcode_12_check_casualties_at_least,
    location_script_opcode_13_check_casualties_at_most,
    location_script_opcode_14_skip_1_operand,
    location_script_opcode_14_skip_1_operand,
    location_script_opcode_00_nop,
    location_script_opcode_17_skip_3_operands,
    location_script_opcode_18_skip_4_operands,
    location_script_opcode_19_request_load_event,
    location_script_opcode_1a_request_choice,
    location_script_opcode_05_skip_2_operands,
    location_script_opcode_1c_set_var,
    location_script_opcode_1d_request_text,
    location_script_opcode_1e_request_deep_dungeon_entry,
    location_script_opcode_1f_request_set_var_2,
    location_script_opcode_20_request_draw_path,
    location_script_opcode_21_request_erase_path,
    location_script_opcode_22_request_draw_location,
    location_script_opcode_23_request_erase_location,
    location_script_opcode_24_focus_location,
    location_script_opcode_25_check_ramza_brave_at_least,
    location_script_opcode_26_check_ramza_brave_at_most,
    location_script_opcode_27_check_ramza_faith_at_least,
    location_script_opcode_28_check_ramza_faith_at_most,
};
