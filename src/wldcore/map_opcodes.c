/* The map script conditional and request opcodes, Ramza checks, bit access, save completion percent, and the script
 * flag list and detail levels. */

#include "fft/bits.h"
#include "fft/brave_story.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/location.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/party.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sound_novel.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/pad.h"
#include "psx/types.h"

static s32 party_find_ramza(void);
static void brave_story_push_fact_level(s32 value);
static void location_script_read_operand_pair(s32* out1, s32* out2);
static void location_script_read_operand_pair_and_date(
    s32* out_month, s32* out_day, s32* out_current_month, s32* out_current_day);

/* Run the map script blocks of `location`'s conditional set in order, each until a condition fails
 * or it stages an action; return 1 at the first staged action with a flag in `action_mask`, else
 * 0. */
s32 location_script_run(s32 location, s32 action_mask) {
    s32 flags;
    u16 i;
    u16 entry;
    s32 offset = g_location_script_base[location] & 0xFFFE;

    g_location_script_state.list = (u16*)((s32)g_location_script_base - (-offset));
    for (i = 0; (entry = g_location_script_state.list[i]) != 0; i++) {
        g_location_script_state.data = (u16*)((s32)g_location_script_base - (-(entry & 0xFFFE)));
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
        if (flags & LOCATION_SCRIPT_RESULT_ACTION_REACHED) {
            if (flags & action_mask) {
                return 1;
            }
        }
    }
    return 0;
}

/* Map script opcodes 0x00 and 0x16: do nothing. */
void location_script_opcode_00_nop(void) {
}

/* Map script opcodes 0x14 and 0x15: skip one operand without testing anything. */
void location_script_opcode_14_skip_1_operand(void) {
    u16* ip = &g_location_script_state.ip;
    *ip += 1;
}

/* Map script opcodes 0x05 to 0x0B and 0x1B: skip two operands without testing anything. */
void location_script_opcode_05_skip_2_operands(void) {
    u16* ip = &g_location_script_state.ip;
    *ip += 2;
}

/* Map script opcode 0x17: skip three operands without testing anything. */
void location_script_opcode_17_skip_3_operands(void) {
    u16* ip = &g_location_script_state.ip;
    *ip += 3;
}

/* Map script opcode 0x18: skip four operands without testing anything. */
void location_script_opcode_18_skip_4_operands(void) {
    u16* ip = &g_location_script_state.ip;
    *ip += 4;
}

/* Reject the Var= condition when the script variable differs from the operand. */
void location_script_opcode_01_check_var_equal(void) {
    s32 variable_id;
    s32 value;

    location_script_read_operand_pair(&variable_id, &value);
    if (script_get_variable(variable_id) != value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Read the next two 16-bit map script operands and step past them. */
static void location_script_read_operand_pair(s32* out1, s32* out2) {
    u16* ip = &g_location_script_state.ip;
    u16 next_ip;

    *out1 = g_location_script_state.data[*ip];
    next_ip = *ip + 1;
    *ip = next_ip;
    *out2 = g_location_script_state.data[next_ip];
    *ip = *ip + 1;
}

/* Reject the Var>= condition when the script variable is below the operand. */
void location_script_opcode_02_check_var_at_least(void) {
    s32 variable_id;
    s32 value;

    location_script_read_operand_pair(&variable_id, &value);
    if (script_get_variable(variable_id) < value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Var<= condition when the script variable is above the operand. */
void location_script_opcode_03_check_var_at_most(void) {
    s32 variable_id;
    s32 value;

    location_script_read_operand_pair(&variable_id, &value);
    if (script_get_variable(variable_id) > value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the HasUnit condition when no roster unit has the operand's sprite set. */
void location_script_opcode_04_check_roster_has_sprite_set(void) {
    s32 i;
    s32 sprite_set;
    u16* ipp;
    u16 ip;
    party_unit_t* unit;
    s32* flags;

    ipp = &g_location_script_state.ip;
    ip = *ipp;
    sprite_set = g_location_script_state.data[ip];
    *ipp = ip + 1;
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = party_get_roster_unit(i);
        if (unit->party_id != PARTY_ID_NONE) {
            if (unit->sprite_set == sprite_set) {
                break;
            }
        }
    }
    if (i == PARTY_ROSTER_SLOT_COUNT) {
        flags = &g_location_script_state.flags;
        *flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Map script opcode 0x0C: skip one operand without testing anything (its own copy of the 0x14
 * body). */
void location_script_opcode_0c_skip_1_operand(void) {
    u16* ip = &g_location_script_state.ip;
    *ip += 1;
}

/* Map script opcode 0x0D: skip one operand without testing anything (its own copy of the 0x14
 * body). */
void location_script_opcode_0d_skip_1_operand(void) {
    u16* ip = &g_location_script_state.ip;
    *ip += 1;
}

/* Reject the Gil>= condition when current war funds are below the operand. */
void location_script_opcode_0e_check_war_funds_at_least(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    u16 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) < value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Gil<= condition when current war funds are above the operand. */
void location_script_opcode_0f_check_war_funds_at_most(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    s32 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (value < script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS)) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Date>= condition when the current date precedes the operand. */
void location_script_opcode_10_check_date_on_or_after(void) {
    s32 month, day, current_month, current_day;
    s32* flags;

    location_script_read_operand_pair_and_date(&month, &day, &current_month, &current_day);
    if ((current_month < month) || ((current_month == month) && (current_day < day))) {
        flags = &g_location_script_state.flags;
        *flags = *flags | LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Read a month and day operand pair from the map script, as
 * location_script_read_operand_pair does, along with the current month and day. */
static void location_script_read_operand_pair_and_date(
    s32* out_month, s32* out_day, s32* out_current_month, s32* out_current_day) {
    u16* ipp;
    u16 ip;

    ipp = &g_location_script_state.ip;
    *out_month = g_location_script_state.data[*ipp];
    ip = *ipp + 1;
    *ipp = ip;
    *out_day = g_location_script_state.data[ip];
    *ipp = *ipp + 1;
    *out_current_month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    *out_current_day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
}

/* Reject the Date<= condition when the current date follows the operand. */
void location_script_opcode_11_check_date_on_or_before(void) {
    s32 month, day, current_month, current_day;
    s32* flags;

    location_script_read_operand_pair_and_date(&month, &day, &current_month, &current_day);
    if ((current_month > month) || ((current_month == month) && (current_day > day))) {
        flags = &g_location_script_state.flags;
        *flags = *flags | LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Casualties>= condition when the current count is below the operand. */
void location_script_opcode_12_check_casualties_at_least(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    u16 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES) < value) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the Casualties<= condition when the current count is above the operand. */
void location_script_opcode_13_check_casualties_at_most(void) {
    u16* ip = &g_location_script_state.ip;
    u16 idx = *ip;
    s32 value = g_location_script_state.data[idx];
    *ip = idx + 1;
    if (value < script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES)) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Stage a deferred LoadEvent request: the event to play and its transition
 * mode (event_transition_push_level). */
void location_script_opcode_19_request_load_event(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_LOAD_EVENT;
}

/* Stage four variable/value pairs for a deferred Choice request. */
void location_script_opcode_1a_request_choice(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        g_location_script_state.choice_vars[i] = g_location_script_state.data[g_location_script_state.ip];
        g_location_script_state.args[i] = g_location_script_state.data[++g_location_script_state.ip];
        g_location_script_state.ip++;
    }
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_CHOICE;
}

/* Map script opcode 0x1C (SetVar): set the script variable named by the first
 * operand to the second. */
void location_script_opcode_1c_set_var(void) {
    s32 variable_id;
    s32 value;

    location_script_read_operand_pair(&variable_id, &value);
    script_set_variable(variable_id, value);
}

/* Stage the text operand for a deferred Text request (opcode 0x1D). */
void location_script_opcode_1d_request_text(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_TEXT;
}

/* Stage the three operands for a deep-dungeon entry request.
 *
 * Each operand advances the script IP before the request becomes ready.
 */
void location_script_opcode_1e_request_deep_dungeon_entry(void) {
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
void location_script_opcode_1f_request_set_var_2(void) {
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
void location_script_opcode_20_request_draw_path(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_DRAW_PATH;
}

/* Stage the two operands for a deferred ErasePath request (opcode 0x21). */
void location_script_opcode_21_request_erase_path(void) {
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.args[1] = g_location_script_state.data[g_location_script_state.ip];
    g_location_script_state.ip++;
    g_location_script_state.flags |= LOCATION_SCRIPT_REQUEST_ERASE_PATH;
}

/* Map script opcode 0x22: stage the location ID for a deferred DrawLocation
 * request. Unlike other opcodes it leaves ip on its operand, which is
 * harmless because staging the request ends the block. */
void location_script_opcode_22_request_draw_location(void) {
    g_location_script_flags |= LOCATION_SCRIPT_REQUEST_DRAW_LOCATION;
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
}

/* Map script opcode 0x23: stage the location ID for a deferred EraseLocation
 * request, leaving ip on its operand like DrawLocation. */
void location_script_opcode_23_request_erase_location(void) {
    g_location_script_flags |= LOCATION_SCRIPT_REQUEST_ERASE_LOCATION;
    g_location_script_state.args[0] = g_location_script_state.data[g_location_script_state.ip];
}

/* Map script opcode 0x24 (FocusLocation): select the operand's location (0xff
 * for the current one) and, only when its dot lies outside the view, request
 * the focus-location level that moves the map to it. It always ends the block
 * and, like DrawLocation, leaves ip on its operand. */
void location_script_opcode_24_focus_location(void) {
    u16 ip = g_location_script_state.ip;
    u16* data = g_location_script_state.data;
    u16 entry;
    s32 index;

    entry = data[ip];
    g_location_script_state.args[0] = entry;
    if (entry == 0xff) {
        index = g_world_map_projection_state.marker.kind;
        g_location_selected = index + 1;
    } else {
        index = entry;
        g_location_selected = index + 1;
    }
    if (world_map_is_outside_pan_bounds(&g_world_map_dots[index].render.position) != 0) {
        s32* flags = &g_location_script_state.flags;

        *flags |= LOCATION_SCRIPT_ACTION_FOCUS_LOCATION;
    }
    {
        s32* flags = &g_location_script_state.flags;

        *flags |= LOCATION_SCRIPT_RESULT_ACTION_REACHED;
    }
}

/* Reject the MCBrave>= condition when Ramza's Brave is below the operand. */
void location_script_opcode_25_check_ramza_brave_at_least(void) {
    party_unit_t* party = party_get_roster_unit(party_find_ramza());
    u16 ip;
    s32 val;

    ip = g_location_script_state.ip;
    val = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (party->bravery < val) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Return the first party slot holding Ramza (any of his chapter sprite sets), or
 * PARTY_MEMBER_SLOT_COUNT when none does. Callers use the result unchecked, so
 * without Ramza the MCBrave and MCFaith checks read roster slot 16, the first
 * guest slot, instead of failing (retail quirk). */
static s32 party_find_ramza(void) {
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

/* Reject the MCBrave<= condition when Ramza's Brave is above the operand. */
void location_script_opcode_26_check_ramza_brave_at_most(void) {
    party_unit_t* party = party_get_roster_unit(party_find_ramza());
    u16 ip;
    s32 val;

    ip = g_location_script_state.ip;
    val = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (val < party->bravery) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the MCFaith>= condition when Ramza's Faith is below the operand. */
void location_script_opcode_27_check_ramza_faith_at_least(void) {
    party_unit_t* party = party_get_roster_unit(party_find_ramza());
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
void location_script_opcode_28_check_ramza_faith_at_most(void) {
    party_unit_t* party = party_get_roster_unit(party_find_ramza());
    u16 ip;
    s32 val;

    ip = g_location_script_state.ip;
    val = g_location_script_state.data[ip];
    g_location_script_state.ip = ip + 1;
    if (val < party->faith) {
        g_location_script_state.flags |= LOCATION_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Set (nonzero `set`) or clear bit `index` of a 32-bit word array. */
void bits_set_value(u32* bits, s32 index, s32 set) {
    u32* base = bits;
    u32 mask;
    s32 word_index = index / 32;

    mask = 1U << (index & 31);
    if (set != 0) {
        base[word_index] |= mask;
    } else {
        base[word_index] &= ~mask;
    }
}

/* Returns whether bit `index` of the little-endian 32-bit array is set.
 *
 * Building the mask before loading the 32-bit value, and ANDing two locals, gives the
 * target's order: expand swaps two register operands, so the loaded 32-bit value ends
 * up as the AND's first operand and shares $v0 with the result. */
s32 bits_test(u32* bits, s32 index) {
    s32 word = index / 32;
    s32 bit = index & 31;
    u32 mask = 1U << bit;
    u32 value = bits[word];

    return (value & mask) != 0;
}

/* Count the set bits among the first 0x1000 read-section bits and return that
 * count as a percentage of 76, the number retail treats as complete. */
s32 sound_novel_calculate_read_percent(void) {
    s32 count = 0;
    s32 i;

    for (i = 0; i < 0x1000; i++) {
        if (bits_test(g_sound_novel_read_section_bits, i) != 0) {
            count++;
        }
    }
    return count * 100 / 76;
}

/* Open the Brave Story Facts list: one row per fact flag set (EVENT_SCRIPT_VAR_FACT_FIRST onward),
 * up to eight, hiding the parent's content panel. */
void brave_story_open_fact_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32 count = 0;
    s32 i = 0;
    s32 record_index;

    flags = g_menu_stack_row_flags;
    values = g_menu_stack_entry_values;
    for (; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
            *values = i + TEXT_ID_FACT_NAME_BASE;
            *flags = 0;
            flags++;
            values++;
            count++;
        }
    }
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x8C;
    origin.x = 0x58;
    layout.visible_rows = i;
    origin.y = 0x50;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_FACTS, count, i);
    window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.content_render].flags
        |= DISPLAY_OBJECT_HIDDEN;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    record_index = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 9;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BRAVE_STORY_FACTS;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the Facts list: block Circle, Cross and Select from the WORLD menu frame
 * again. */
void brave_story_restore_fact_list_buttons(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void brave_story_block_fact_list_buttons(menu_stack_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the Facts list; twin of tutorial_handle_topic_list_input
 * with help message 0x108f, handing a confirmed entry's value to
 * brave_story_push_fact_level. */
void brave_story_handle_fact_list_input(menu_stack_panel_level_t* level) {
    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_FACTS);
            brave_story_push_fact_level(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_FACTS);
        menu_stack_dispatch_resume();
        return;
    }
    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) && g_file_still_loading == 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        brave_story_block_fact_list_buttons(level);
        menu_stack_push_message_level(0x108F, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_file_still_loading == 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes MENU_STACK_LEVEL_BRAVE_STORY_FACT, the text of one fact, which
 * brave_story_wait_fact_then_pop closes: records the entry on the new
 * level, sets up g_text_window on its text
 * (TEXT_ID_WORLD_MESSAGE_BASE + entry) with no substitutions, hides the Brave
 * Story menu's four windows and frame, and says
 * TEXT_ID_BRAVE_STORY_ENTRY_INFO with the fact's name
 * (TEXT_ID_FACT_NAME_BASE + entry).
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The depth is spelled as the global at every use rather than cached in a
 * local: the target re-reads g_menu_stack_depth after each call. */
static void brave_story_push_fact_level(s32 value) {
    s32 coordinate_mode = 1;

    g_menu_stack_records[g_menu_stack_depth].window_index = value;
    g_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_text_window.image_x = 0;
        g_text_window.image_y = 0x60;
        break;
    case 1:
        g_text_window.image_x = 0;
        g_text_window.image_y = 0x70;
        break;
    }
    g_text_window.base.x = -122;
    g_text_window.base.y = -40;
    g_text_window.text_width = 0xdc;
    g_text_window.rows_per_page = 8;
    g_text_window.priority = 9;
    g_text_window.image_coordinate_mode = coordinate_mode;
    g_text_window.extra_render_index = -1;
    g_text_window.text_substitutions[1] = -1;
    g_text_window.text_substitutions[0] = -1;
    text_window_open(&g_text_window);
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.main_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.side_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.frame_render].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.upper_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].list_window.lower_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_FACT_NAME_BASE;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_BRAVE_STORY_FACT;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Steps a fact's text window until text_window_step reports it
 * closed, then pops the level, shows the Brave Story menu's windows and
 * frame again and reopens the Facts list (brave_story_open_fact_list). */
void brave_story_wait_fact_then_pop(void) {
    s32 depth;

    if (!(g_window_gs_image_records[g_text_window.render_record_index].flags & DISPLAY_OBJECT_POP_IN)
        && text_window_step(&g_text_window) == 0) {
        depth = g_menu_stack_depth;
        g_window_gs_image_records[g_menu_stack_records[depth - 2].list_window.frame_render].flags
            |= DISPLAY_OBJECT_POP_IN;
        g_window_records[g_menu_stack_records[depth - 2].list_window.main_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_records[g_menu_stack_records[depth - 2].list_window.side_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[g_menu_stack_records[depth - 2].list_window.frame_render].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_records[g_menu_stack_records[depth - 2].list_window.upper_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_menu_stack_depth = depth - 1;
        g_window_records[g_menu_stack_records[depth - 2].list_window.lower_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        brave_story_open_fact_list();
    }
}
