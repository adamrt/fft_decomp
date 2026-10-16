/* The map script conditional and request opcodes, Ramza checks, bit access, save completion percent, and the script
 * flag list and detail levels. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/wldcore.h"
#include "fft/wldcore_map.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_script.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

static s32 wldcore_get_ramza_roster_index(void);
static void wldcore_menu_push_script_flag_01a4_detail_level(s32 value);
static void wldcore_script_read_operand_pair(s32* out1, s32* out2);
static void wldcore_script_read_operand_pair_and_date(
    s32* out_month, s32* out_day, s32* out_current_month, s32* out_current_day);

s32 wldcore_script_process_conditional_set(s32 location, s32 action_mask) {
    s32 flags;
    u16 i;
    u16 entry;
    s32 offset = g_wldcore_script_base[location] & 0xFFFE;

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

static void wldcore_script_read_operand_pair(s32* out1, s32* out2) {
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

static void wldcore_script_read_operand_pair_and_date(
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
    party_unit_t* party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    u16 ip;
    s32 val;

    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (party->bravery < val) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

static s32 wldcore_get_ramza_roster_index(void) {
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
    party_unit_t* party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    u16 ip;
    s32 val;

    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (val < party->bravery) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the MCFaith>= condition when Ramza's Faith is below the operand. */
void wldcore_opcode_map_check_ramza_faith_at_least(void) {
    party_unit_t* party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    u16 ip;
    s32 val;

    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (party->faith < val) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the MCFaith<= condition when Ramza's Faith is above the operand. */
void wldcore_opcode_map_check_ramza_faith_at_most(void) {
    party_unit_t* party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    u16 ip;
    s32 val;

    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (val < party->faith) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

void wldcore_set_bit_value(u32* bits, s32 index, s32 set) {
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
s32 wldcore_test_bit(u32* bits, s32 index) {
    s32 word = index / 32;
    s32 bit = index & 31;
    u32 mask = 1U << bit;
    u32 value = bits[word];

    return (value & mask) != 0;
}

/* Count the set bits among the first 0x1000 saved-data bits and return that
 * count as a percentage of 76. */
s32 wldcore_script_calculate_save_data_completion_percent(void) {
    s32 count = 0;
    s32 i;

    for (i = 0; i < 0x1000; i++) {
        if (wldcore_test_bit(g_main_saved_data_bits, i) != 0) {
            count++;
        }
    }
    return count * 100 / 76;
}

void wldcore_list_open_script_flags_01a4_01bb(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32 count = 0;
    s32 i = 0;
    s32 record_index;

    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    for (; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
            *values = i + TEXT_ID_FACT_NAME_BASE;
            *flags = 0;
            flags++;
            values++;
            count++;
        }
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x8C;
    origin.x = 0x58;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_SCRIPT_FLAGS_01A4_01BB, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.content_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    record_index = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_SCRIPT_FLAGS_01A4_01BB;
    g_wldcore_menu_stack_depth = record_index + 1;
}

void wldcore_list_script_flags_restore_blocked_buttons(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_script_flags_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* List level input step for cursor slot 20; twin of wldcore_list_handle_tutorial_topics_input with help
 * message 0x108f, handing a confirmed entry's value to 0x80092618. */
void wldcore_list_handle_script_flags_01a4_01bb_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_SCRIPT_FLAGS_01A4_01BB);
            wldcore_menu_push_script_flag_01a4_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_SCRIPT_FLAGS_01A4_01BB);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_script_flags_block_all_buttons(level);
        wldcore_menu_push_message_level(0x108F, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes the script-variable detail text window as menu level 0x2f, the
 * partner of the wldcore_menu_close_script_flag_01a4_detail_level pop: records the entry on the new level, sets
 * up the shared scrollable text window at g_wldcore_scrollable_text_window for text 0x8800 + entry
 * with no substitutions, then hides the parent list-window level's four
 * window records and its frame render record with flag 0x10 and starts WORLD
 * thread 14 on text 0xb849 with substitution value 0x9800 + entry.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The depth is spelled as the global at every use rather than cached in a
 * local: the target re-reads g_wldcore_menu_stack_depth after each call. */
static void wldcore_menu_push_script_flag_01a4_detail_level(s32 value) {
    s32 coordinate_mode = 1;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_index = value;
    g_wldcore_scrollable_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x60;
        break;
    case 1:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        break;
    }
    g_wldcore_scrollable_text_window.base.x = -122;
    g_wldcore_scrollable_text_window.base.y = -40;
    g_wldcore_scrollable_text_window.text_width = 0xdc;
    g_wldcore_scrollable_text_window.rows_per_page = 8;
    g_wldcore_scrollable_text_window.priority = 9;
    g_wldcore_scrollable_text_window.image_coordinate_mode = coordinate_mode;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = -1;
    wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.main_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.side_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.frame_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.upper_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.lower_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_FACT_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_SCRIPT_FLAG_01A4_DETAIL;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

/* Scrollable text window also stepped by wldcore_menu_wait_tutorial_topic_text_then_pop_level. */

/* Steps the text window until 0x800903e4 reports it finished, then pops the
 * level, restores the parent list window's windows and frame render record,
 * and reopens the script-variable list of 0x80092208. */
void wldcore_menu_close_script_flag_01a4_detail_level(void) {
    s32 depth;

    if (!(g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags
            & WORLD_RENDER_RECORD_POP_IN)
        && wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
            |= WORLD_RENDER_RECORD_POP_IN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.main_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.side_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.upper_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_menu_stack_depth = depth - 1;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.lower_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        wldcore_list_open_script_flags_01a4_01bb();
    }
}
