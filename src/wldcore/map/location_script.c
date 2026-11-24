/* Location scripts test story conditions and request world-map events, choices and visible routes. */

#include "fft/game/character_identity.h"
#include "fft/game/party.h"
#include "fft/game/party_unit.h"
#include "fft/script/script_variable.h"
#include "fft/world/location.h"
#include "fft/world/world_map.h"
#include "psx/types.h"

/* Opcode halfword of a location script instruction: the index of its handler in s_location_script_opcodes.
 * The opcodes that only skip their operands test nothing; a suffix tells their copies apart. */
typedef enum location_opcode {
    LOCATION_OPCODE_NOP_00 = 0x00,
    LOCATION_OPCODE_CHECK_VAR_EQUAL = 0x01,
    LOCATION_OPCODE_CHECK_VAR_AT_LEAST = 0x02,
    LOCATION_OPCODE_CHECK_VAR_AT_MOST = 0x03,
    LOCATION_OPCODE_CHECK_ROSTER_HAS_CHARACTER = 0x04,
    LOCATION_OPCODE_SKIP_2_OPERANDS_05 = 0x05,
    LOCATION_OPCODE_SKIP_2_OPERANDS_06 = 0x06,
    LOCATION_OPCODE_SKIP_2_OPERANDS_07 = 0x07,
    LOCATION_OPCODE_SKIP_2_OPERANDS_08 = 0x08,
    LOCATION_OPCODE_SKIP_2_OPERANDS_09 = 0x09,
    LOCATION_OPCODE_SKIP_2_OPERANDS_0A = 0x0a,
    LOCATION_OPCODE_SKIP_2_OPERANDS_0B = 0x0b,
    LOCATION_OPCODE_SKIP_1_OPERAND_0C = 0x0c,
    LOCATION_OPCODE_SKIP_1_OPERAND_0D = 0x0d,
    LOCATION_OPCODE_CHECK_WAR_FUNDS_AT_LEAST = 0x0e,
    LOCATION_OPCODE_CHECK_WAR_FUNDS_AT_MOST = 0x0f,
    LOCATION_OPCODE_CHECK_DATE_ON_OR_AFTER = 0x10,
    LOCATION_OPCODE_CHECK_DATE_ON_OR_BEFORE = 0x11,
    LOCATION_OPCODE_CHECK_CASUALTIES_AT_LEAST = 0x12,
    LOCATION_OPCODE_CHECK_CASUALTIES_AT_MOST = 0x13,
    LOCATION_OPCODE_SKIP_1_OPERAND_14 = 0x14,
    LOCATION_OPCODE_SKIP_1_OPERAND_15 = 0x15,
    LOCATION_OPCODE_NOP_16 = 0x16,
    LOCATION_OPCODE_SKIP_3_OPERANDS = 0x17,
    LOCATION_OPCODE_SKIP_4_OPERANDS = 0x18,
    LOCATION_OPCODE_REQUEST_LOAD_EVENT = 0x19,
    LOCATION_OPCODE_REQUEST_CHOICE = 0x1a,
    LOCATION_OPCODE_SKIP_2_OPERANDS_1B = 0x1b,
    LOCATION_OPCODE_SET_VAR = 0x1c,
    LOCATION_OPCODE_REQUEST_TEXT = 0x1d,
    LOCATION_OPCODE_REQUEST_DEEP_DUNGEON_ENTRY = 0x1e,
    LOCATION_OPCODE_REQUEST_SET_VAR = 0x1f,
    LOCATION_OPCODE_REQUEST_DRAW_PATH = 0x20,
    LOCATION_OPCODE_REQUEST_ERASE_PATH = 0x21,
    LOCATION_OPCODE_REQUEST_DRAW_LOCATION = 0x22,
    LOCATION_OPCODE_REQUEST_ERASE_LOCATION = 0x23,
    LOCATION_OPCODE_FOCUS_LOCATION = 0x24,
    LOCATION_OPCODE_CHECK_RAMZA_BRAVE_AT_LEAST = 0x25,
    LOCATION_OPCODE_CHECK_RAMZA_BRAVE_AT_MOST = 0x26,
    LOCATION_OPCODE_CHECK_RAMZA_FAITH_AT_LEAST = 0x27,
    LOCATION_OPCODE_CHECK_RAMZA_FAITH_AT_MOST = 0x28
} location_opcode_e;

static void location_opcode_nop(void);
static void location_opcode_skip_1_operand(void);
static void location_opcode_skip_2_operands(void);
static void location_opcode_skip_3_operands(void);
static void location_opcode_skip_4_operands(void);
static void location_opcode_check_var_equal(void);
static void location_opcode_check_var_at_least(void);
static void location_opcode_check_var_at_most(void);
static void location_opcode_check_roster_has_character(void);
static void location_opcode_skip_1_operand_0c(void);
static void location_opcode_skip_1_operand_0d(void);
static void location_opcode_check_war_funds_at_least(void);
static void location_opcode_check_war_funds_at_most(void);
static void location_opcode_check_date_on_or_after(void);
static void location_opcode_check_date_on_or_before(void);
static void location_opcode_check_casualties_at_least(void);
static void location_opcode_check_casualties_at_most(void);
static void location_opcode_request_load_event(void);
static void location_opcode_request_choice(void);
static void location_opcode_set_var(void);
static void location_opcode_request_text(void);
static void location_opcode_request_deep_dungeon_entry(void);
static void location_opcode_request_set_var(void);
static void location_opcode_request_draw_path(void);
static void location_opcode_request_erase_path(void);
static void location_opcode_request_draw_location(void);
static void location_opcode_request_erase_location(void);
static void location_opcode_focus_location(void);
static void location_opcode_check_ramza_brave_at_least(void);
static void location_opcode_check_ramza_brave_at_most(void);
static void location_opcode_check_ramza_faith_at_least(void);
static void location_opcode_check_ramza_faith_at_most(void);

static void read_operand_pair(s32* out1, s32* out2);
static void read_operand_dates(s32* out_month, s32* out_day, s32* out_current_month, s32* out_current_day);
static s32 find_ramza_slot(void);

/* Opcode-indexed dispatch, defined after its handlers below. */
static void (*s_location_script_opcodes[41])(void);

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
            s_location_script_opcodes[op]();
        } while (!(g_location_script_state.flags
            & (LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_RESULT_CONDITION_FAILED)));
        flags = g_location_script_state.flags;
        if ((flags & LOCATION_SCRIPT_RESULT_ACTION_REACHED) && (flags & action_mask)) {
            return 1;
        }
    }
    return 0;
}

/* LOCATION_OPCODE_NOP_00 (0x00)
 * LOCATION_OPCODE_NOP_16 (0x16)
 * Do nothing. */
static void location_opcode_nop(void) {
}

/* LOCATION_OPCODE_SKIP_1_OPERAND_14 (0x14)
 * LOCATION_OPCODE_SKIP_1_OPERAND_15 (0x15)
 * Skip one operand without testing anything. */
static void location_opcode_skip_1_operand(void) {
    g_location_script_state.ip += 1;
}

/* LOCATION_OPCODE_SKIP_2_OPERANDS_05 (0x05)
 * LOCATION_OPCODE_SKIP_2_OPERANDS_06 (0x06)
 * LOCATION_OPCODE_SKIP_2_OPERANDS_07 (0x07)
 * LOCATION_OPCODE_SKIP_2_OPERANDS_08 (0x08)
 * LOCATION_OPCODE_SKIP_2_OPERANDS_09 (0x09)
 * LOCATION_OPCODE_SKIP_2_OPERANDS_0A (0x0a)
 * LOCATION_OPCODE_SKIP_2_OPERANDS_0B (0x0b)
 * LOCATION_OPCODE_SKIP_2_OPERANDS_1B (0x1b)
 * Skip two operands without testing anything. */
static void location_opcode_skip_2_operands(void) {
    g_location_script_state.ip += 2;
}

/* LOCATION_OPCODE_SKIP_3_OPERANDS (0x17)
 * Skip three operands without testing anything. */
static void location_opcode_skip_3_operands(void) {
    g_location_script_state.ip += 3;
}

/* LOCATION_OPCODE_SKIP_4_OPERANDS (0x18)
 * Skip four operands without testing anything. */
static void location_opcode_skip_4_operands(void) {
    g_location_script_state.ip += 4;
}

/* LOCATION_OPCODE_CHECK_VAR_EQUAL (0x01)
 * Reject the Var= condition when the script variable differs from the operand. */
static void location_opcode_check_var_equal(void) {
    s32 variable_id;
    s32 value;

    read_operand_pair(&variable_id, &value);
    if (script_variable_get(variable_id) != value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Read the next two 16-bit map script operands and step past them. */
static void read_operand_pair(s32* out1, s32* out2) {
    u16* ip = &g_location_script_state.ip;
    u16 next_ip;

    *out1 = g_location_script_state.data[*ip];
    next_ip = *ip + 1;
    *ip = next_ip;
    *out2 = g_location_script_state.data[next_ip];
    (*ip)++;
}

/* LOCATION_OPCODE_CHECK_VAR_AT_LEAST (0x02)
 * Reject the Var>= condition when the script variable is below the operand. */
static void location_opcode_check_var_at_least(void) {
    s32 variable_id;
    s32 value;

    read_operand_pair(&variable_id, &value);
    if (script_variable_get(variable_id) < value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* LOCATION_OPCODE_CHECK_VAR_AT_MOST (0x03)
 * Reject the Var<= condition when the script variable is above the operand. */
static void location_opcode_check_var_at_most(void) {
    s32 variable_id;
    s32 value;

    read_operand_pair(&variable_id, &value);
    if (script_variable_get(variable_id) > value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* LOCATION_OPCODE_CHECK_ROSTER_HAS_CHARACTER (0x04)
 * Reject HasUnit when no occupied roster slot has the operand's character identity.
 * The native scripts test Beowulf, joined Mustadio and Dragon Reis here. */
static void location_opcode_check_roster_has_character(void) {
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
        unit = party_get_roster_unit(i);
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

/* LOCATION_OPCODE_SKIP_1_OPERAND_0C (0x0c)
 * Skip one operand without testing anything (its own copy of the 0x14
 * body). */
static void location_opcode_skip_1_operand_0c(void) {
    g_location_script_state.ip += 1;
}

/* LOCATION_OPCODE_SKIP_1_OPERAND_0D (0x0d)
 * Skip one operand without testing anything (its own copy of the 0x14
 * body). */
static void location_opcode_skip_1_operand_0d(void) {
    g_location_script_state.ip += 1;
}

/* LOCATION_OPCODE_CHECK_WAR_FUNDS_AT_LEAST (0x0e)
 * Reject the Gil>= condition when current war funds are below the operand. */
static void location_opcode_check_war_funds_at_least(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    u16 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (script_variable_get(SCRIPT_VAR_WAR_FUNDS) < value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* LOCATION_OPCODE_CHECK_WAR_FUNDS_AT_MOST (0x0f)
 * Reject the Gil<= condition when current war funds are above the operand. */
static void location_opcode_check_war_funds_at_most(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    s32 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (value < script_variable_get(SCRIPT_VAR_WAR_FUNDS)) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* LOCATION_OPCODE_CHECK_DATE_ON_OR_AFTER (0x10)
 * Reject the Date>= condition when the current date precedes the operand. */
static void location_opcode_check_date_on_or_after(void) {
    s32 month, day, current_month, current_day;

    read_operand_dates(&month, &day, &current_month, &current_day);
    if (current_month < month || (current_month == month && current_day < day)) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Read a month and day operand pair from the map script, as
 * read_operand_pair does, along with the current month and day. */
static void read_operand_dates(s32* out_month, s32* out_day, s32* out_current_month, s32* out_current_day) {
    u16* ipp;
    u16 ip;

    ipp = &g_location_script_state.ip;
    *out_month = g_location_script_state.data[*ipp];
    ip = *ipp + 1;
    *ipp = ip;
    *out_day = g_location_script_state.data[ip];
    (*ipp)++;
    *out_current_month = script_variable_get(SCRIPT_VAR_MONTH);
    *out_current_day = script_variable_get(SCRIPT_VAR_DAY);
}

/* LOCATION_OPCODE_CHECK_DATE_ON_OR_BEFORE (0x11)
 * Reject the Date<= condition when the current date follows the operand. */
static void location_opcode_check_date_on_or_before(void) {
    s32 month, day, current_month, current_day;

    read_operand_dates(&month, &day, &current_month, &current_day);
    if (current_month > month || (current_month == month && current_day > day)) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* LOCATION_OPCODE_CHECK_CASUALTIES_AT_LEAST (0x12)
 * Reject the Casualties>= condition when the current count is below the operand. */
static void location_opcode_check_casualties_at_least(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    u16 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (script_variable_get(SCRIPT_VAR_CASUALTIES) < value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* LOCATION_OPCODE_CHECK_CASUALTIES_AT_MOST (0x13)
 * Reject the Casualties<= condition when the current count is above the operand. */
static void location_opcode_check_casualties_at_most(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    s32 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (value < script_variable_get(SCRIPT_VAR_CASUALTIES)) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* LOCATION_OPCODE_REQUEST_LOAD_EVENT (0x19)
 * Stage a deferred LoadEvent request: the event to play and its transition
 * mode (world_map_open_event_transition). */
static void location_opcode_request_load_event(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_LOAD_EVENT;
}

/* LOCATION_OPCODE_REQUEST_CHOICE (0x1a)
 * Stage four battle-map/event pairs for a deferred Choice request. */
static void location_opcode_request_choice(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        g_location_script_state.choice_map_ids[i] = g_location_script_state.data[g_location_script_state.ip];
        g_location_script_state.args[i] = g_location_script_state.data[++g_location_script_state.ip];
        g_location_script_state.ip++;
    }
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_CHOICE;
}

/* LOCATION_OPCODE_SET_VAR (0x1c)
 * Set the script variable named by the first
 * operand to the second. */
static void location_opcode_set_var(void) {
    s32 variable_id;
    s32 value;

    read_operand_pair(&variable_id, &value);
    script_variable_set(variable_id, value);
}

/* LOCATION_OPCODE_REQUEST_TEXT (0x1d)
 * Stage the text operand for a deferred Text request. */
static void location_opcode_request_text(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_TEXT;
}

/* LOCATION_OPCODE_REQUEST_DEEP_DUNGEON_ENTRY (0x1e)
 * Stage the three operands for a deep-dungeon entry request.
 *
 * Each operand advances the script IP before the request becomes ready.
 */
static void location_opcode_request_deep_dungeon_entry(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[2] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_DEEP_DUNGEON_ENTRY;
}

/* LOCATION_OPCODE_REQUEST_SET_VAR (0x1f)
 * Stage the two operands for the deferred SetVar2 request.
 *
 * This opcode only prepares the request; a later dispatcher performs it.
 */
static void location_opcode_request_set_var(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_SET_VAR_2;
}

/* LOCATION_OPCODE_REQUEST_DRAW_PATH (0x20)
 * Stage the two operands for a deferred DrawPath request.
 *
 * The request flag is published after both operands and IP updates.
 */
static void location_opcode_request_draw_path(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_DRAW_PATH;
}

/* LOCATION_OPCODE_REQUEST_ERASE_PATH (0x21)
 * Stage the two operands for a deferred ErasePath request. */
static void location_opcode_request_erase_path(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_ERASE_PATH;
}

/* LOCATION_OPCODE_REQUEST_DRAW_LOCATION (0x22)
 * Stage the location ID for a deferred DrawLocation
 * request. Unlike other opcodes it leaves ip on its operand, which is
 * harmless because staging the request ends the block. */
static void location_opcode_request_draw_location(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_DRAW_LOCATION;
}

/* LOCATION_OPCODE_REQUEST_ERASE_LOCATION (0x23)
 * Stage the location ID for a deferred EraseLocation
 * request, leaving ip on its operand like DrawLocation. */
static void location_opcode_request_erase_location(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_ERASE_LOCATION;
}

/* LOCATION_OPCODE_FOCUS_LOCATION (0x24)
 * Select the operand's location (0xff
 * for the current one) and, only when its dot lies outside the view, request
 * the focus-location level that moves the map to it. It always ends the block
 * and, like DrawLocation, leaves ip on its operand. */
static void location_opcode_focus_location(void) {
    u16 ip = g_location_script_state.ip;
    u16* data = g_location_script_state.data;
    u16 entry;
    s32 index;

    entry = data[ip];
    g_location_script_state.args[0] = entry;
    if (entry == 0xff) {
        index = g_world_map_marker_state.marker.location_id;
    } else {
        index = entry;
    }
    g_location_selected = index + 1;
    if (world_map_point_needs_pan(&g_world_map_dots[index].render.position)) {
        g_location_script_state.flags |= LOCATION_SCRIPT_ACTION_FOCUS_LOCATION;
    }
    g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_ACTION_REACHED;
}

/* LOCATION_OPCODE_CHECK_RAMZA_BRAVE_AT_LEAST (0x25)
 * Reject the MCBrave>= condition when Ramza's Brave is below the operand. */
static void location_opcode_check_ramza_brave_at_least(void) {
    party_unit_t* party = party_get_roster_unit(find_ramza_slot());
    u16 ip;
    s32 threshold;

    ip = g_location_script_state.ip;
    threshold = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (party->brave < threshold) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Return the first party slot holding Ramza (any of his chapter sprite sets), or
 * PARTY_MEMBER_SLOT_COUNT when none does. Callers use the result unchecked, so
 * without Ramza the MCBrave and MCFaith checks read roster slot 16, the first
 * guest slot, instead of failing (retail quirk). */
static s32 find_ramza_slot(void) {
    s32 i;
    for (i = 0; i < PARTY_MEMBER_SLOT_COUNT; i++) {
        party_unit_t* party = party_get_roster_unit(i);
        if (party->party_id != PARTY_ID_NONE && party->sprite_set != CHARACTER_IDENTITY_ENTD_NONE
            && party->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
            return i;
        }
    }
    return i;
}

/* LOCATION_OPCODE_CHECK_RAMZA_BRAVE_AT_MOST (0x26)
 * Reject the MCBrave<= condition when Ramza's Brave is above the operand. */
static void location_opcode_check_ramza_brave_at_most(void) {
    party_unit_t* party = party_get_roster_unit(find_ramza_slot());
    u16 ip;
    s32 threshold;

    ip = g_location_script_state.ip;
    threshold = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (threshold < party->brave) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* LOCATION_OPCODE_CHECK_RAMZA_FAITH_AT_LEAST (0x27)
 * Reject the MCFaith>= condition when Ramza's Faith is below the operand. */
static void location_opcode_check_ramza_faith_at_least(void) {
    party_unit_t* party = party_get_roster_unit(find_ramza_slot());
    u16 ip;
    s32 threshold;

    ip = g_location_script_state.ip;
    threshold = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (party->faith < threshold) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* LOCATION_OPCODE_CHECK_RAMZA_FAITH_AT_MOST (0x28)
 * Reject the MCFaith<= condition when Ramza's Faith is above the operand. */
static void location_opcode_check_ramza_faith_at_most(void) {
    party_unit_t* party = party_get_roster_unit(find_ramza_slot());
    u16 ip;
    s32 threshold;

    ip = g_location_script_state.ip;
    threshold = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (threshold < party->faith) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Original opcode table; repeated entries retain their shared skip handlers. */
static void (*s_location_script_opcodes[41])(void) = {
    location_opcode_nop,
    location_opcode_check_var_equal,
    location_opcode_check_var_at_least,
    location_opcode_check_var_at_most,
    location_opcode_check_roster_has_character,
    location_opcode_skip_2_operands,
    location_opcode_skip_2_operands,
    location_opcode_skip_2_operands,
    location_opcode_skip_2_operands,
    location_opcode_skip_2_operands,
    location_opcode_skip_2_operands,
    location_opcode_skip_2_operands,
    location_opcode_skip_1_operand_0c,
    location_opcode_skip_1_operand_0d,
    location_opcode_check_war_funds_at_least,
    location_opcode_check_war_funds_at_most,
    location_opcode_check_date_on_or_after,
    location_opcode_check_date_on_or_before,
    location_opcode_check_casualties_at_least,
    location_opcode_check_casualties_at_most,
    location_opcode_skip_1_operand,
    location_opcode_skip_1_operand,
    location_opcode_nop,
    location_opcode_skip_3_operands,
    location_opcode_skip_4_operands,
    location_opcode_request_load_event,
    location_opcode_request_choice,
    location_opcode_skip_2_operands,
    location_opcode_set_var,
    location_opcode_request_text,
    location_opcode_request_deep_dungeon_entry,
    location_opcode_request_set_var,
    location_opcode_request_draw_path,
    location_opcode_request_erase_path,
    location_opcode_request_draw_location,
    location_opcode_request_erase_location,
    location_opcode_focus_location,
    location_opcode_check_ramza_brave_at_least,
    location_opcode_check_ramza_brave_at_most,
    location_opcode_check_ramza_faith_at_least,
    location_opcode_check_ramza_faith_at_most,
};
