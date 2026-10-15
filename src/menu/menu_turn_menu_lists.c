/* Ability/skillset list building and the turn menu threads. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/types.h"

/* Widen a 0xff-terminated byte list without copying the terminator; return the count. */
s32 menu_copy_u8_to_u16_terminated(u16* destination, const u8* source) {
    s32 count = 0;

    for (;;) {
        if (*source == 0xFF) {
            return count;
        }
        *destination = *source;
        source++;
        count++;
        destination++;
    }
}

/* Widen count bytes into 16-bit menu values; a nonpositive count writes nothing. */
void menu_copy_u8_to_u16(u16* destination, const u8* source, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        *destination = *source;
        source++;
        destination++;
    }
}

/* Builds the ability list for the active skillset menu entry (menu entry 4's
 * selected_index selects the skillset; g_action_menu_row_types gives its list type). Each type
 * fills the 0x52-row work buffer at *g_ai_workspace_ptr through its own loader, then
 * the rows are sized, grayed (not enough MP, or silenced for voice abilities)
 * and the window layout at 0x80166994 is set up for 6 visible rows.
 * BATTLE twin of menu_build_ability_list (0x800f32e4); the two are masked-identical.
 *
 * Matching notes: the layout block is one struct (its x/y are s16, so `-= 4`
 * stays an addiu immediate and the constant is not shared); window_x is a
 * separate local from the unit index, and its updates follow the loader
 * calls in each arm. */
void menu_build_ability_list(s32 mode) {
    s32 unit_index;
    s32 window_x;
    unit_stats_t* unit;
    s32 i;
    s32 type;
    s32 skillset;
    s32 count;
    s32 found;
    s32 columns;
    s32 row_mode;
    s32 row_flags;

    g_menu_ability_layout.columns.text_ids[0] = (s16*)g_ai_workspace_ptr->ability_list.ids;
    g_menu_ability_layout.columns.text_ids[1] = (s16*)g_ai_workspace_ptr->ability_list.values;
    g_menu_ability_layout.columns.text_ids[2] = (s16*)g_ai_workspace_ptr->ability_list.extras;
    unit_index = g_menu_selected_unit.battle_id;
    unit = menu_get_selected_unit_stats();
    type = g_action_menu_row_types[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index];
    skillset = g_menu_skillsets[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index];
    g_menu_selected_action.skillset_id = skillset;
    for (i = 0; i < 0x50; i++) {
        g_ai_workspace_ptr->ability_list.ids[i] = 0xFFFF;
        g_ai_workspace_ptr->ability_list.mp_costs[i] = 0xFF;
        g_ai_workspace_ptr->ability_list.ct[i] = 1;
        g_ai_workspace_ptr->ability_list.values[i] = 0;
        g_ai_workspace_ptr->ability_list.extras[i] = 0xFFFF;
        g_ai_workspace_ptr->ability_list.flags[i] = 0;
        g_ai_workspace_ptr->ability_list.turns[i] = 1;
        g_ai_workspace_ptr->ability_list.skill_ct[i] = 0xFF;
        g_menu_ability_display_flags[i] = 0;
    }
    g_ai_workspace_ptr->ability_list.mp_costs[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.ct[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.flags[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.turns[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.skill_ct[0x50] = 0xFF;

    if (type == ACTION_MENU_TYPE_DEFAULT) {
        battle_menu_get_unit_skillset_ability_data(unit_index, skillset, (s16*)g_ai_workspace_ptr->ability_list.ids,
            g_ai_workspace_ptr->ability_list.mp_costs, g_ai_workspace_ptr->ability_list.ct, 0,
            g_ai_workspace_ptr->ability_list.flags, g_ai_workspace_ptr->ability_list.turns);
        mem_copy_bytes_nonempty(g_ai_workspace_ptr->ability_list.skill_ct, g_ai_workspace_ptr->ability_list.ct, 0x50);
    }
    if (type == ACTION_MENU_TYPE_ITEM_INVENTORY) {
        battle_menu_display_item_inventory_ability(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        battle_menu_load_throw_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_ARITHMETICKS) {
        if (mode == MENU_ARITHMETICKS_LIST_ATTRIBUTES) {
            battle_menu_load_math_skill_attributes(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
        if (mode == MENU_ARITHMETICKS_LIST_MULTIPLES) {
            battle_menu_load_math_skill_multiples(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
        if (mode == MENU_ARITHMETICKS_LIST_ABILITIES) {
            battle_menu_collect_calculator_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
    }
    if (type == ACTION_MENU_TYPE_MONSTER) {
        battle_menu_collect_monster_skill_abilities(
            unit_index, skillset, g_ai_workspace_ptr->ability_list.ids, 0, g_ai_workspace_ptr->ability_list.flags);
    }
    if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        battle_menu_load_draw_out_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_CHARGE) {
        battle_menu_load_charge_skillset(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids,
            g_ai_workspace_ptr->ability_list.mp_costs, g_ai_workspace_ptr->ability_list.turns);
        mem_copy_bytes_nonempty(
            g_ai_workspace_ptr->ability_list.skill_ct, g_ai_workspace_ptr->ability_list.mp_costs, 0x50);
    }
    window_x = 0xAC;
    if (type != ACTION_MENU_TYPE_ARITHMETICKS) {
        mode = MENU_ARITHMETICKS_LIST_ABILITIES;
    }

    g_menu_ability_layout.columns.mode[1] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_menu_ability_layout.columns.x[1] = 0x6A;
    g_menu_ability_layout.columns.x[2] = 0x7E;
    g_menu_ability_layout.columns.mode[0] = MENU_TEXT_COLUMN_TEXT;
    if (g_menu_action_type_modes[type][1] == 1) { /* lists by id */
        count = menu_copy_u8_to_u16_terminated(
            g_ai_workspace_ptr->ability_list.ids, g_ai_workspace_ptr->ability_list.mp_costs);
        menu_copy_u8_to_u16(g_ai_workspace_ptr->ability_list.extras, g_ai_workspace_ptr->ability_list.turns, count);
        found = 0;
        for (i = 0; i < count; i++) {
            g_ai_workspace_ptr->ability_list.values[i]
                = g_main_item_quantities[g_ai_workspace_ptr->ability_list.ids[i]];
            if (g_ai_workspace_ptr->ability_list.values[i] == 0) {
                g_menu_ability_display_flags[i] = 4;
            } else {
                found = 1;
            }
        }
        row_mode = 7;
        if (found != 0) {
            window_x -= 4;
        } else {
            g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
            menu_copy_u8_to_u16(g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.turns, count);
            window_x = 0x94;
            row_mode = 8;
        }
        row_flags = TEXT_ID_ITEM_NAME_BASE;
        g_menu_ability_layout.columns.x[1] -= 4;
        g_menu_ability_layout.columns.x[2] -= 4;
        columns = 0x11;
        if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
            columns = 0x17;
        }
    } else {
        found = 0;
        for (count = 0; count < 0x40; count++) {
            if (g_ai_workspace_ptr->ability_list.ids[count] == 0xFFFF) {
                break;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            for (i = 0; i < count; i++) {
                if (g_ai_workspace_ptr->ability_list.mp_costs[i] != 0) {
                    found = 1;
                    break;
                }
            }
        }
        if (type == ACTION_MENU_TYPE_ARITHMETICKS && mode > 0) {
            window_x = 0x88;
            g_menu_ability_layout.columns.mode[1] = MENU_TEXT_COLUMN_HIDDEN;
            g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
            row_mode = 9;
        } else {
            if (found != 0 && type == ACTION_MENU_TYPE_DEFAULT) {
                menu_copy_u8_to_u16_terminated(
                    g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.mp_costs);
                window_x -= 8;
                menu_copy_u8_to_u16(
                    g_ai_workspace_ptr->ability_list.extras, g_ai_workspace_ptr->ability_list.turns, count);
                row_mode = 6;
                g_menu_ability_layout.columns.x[1] -= 8;
                g_menu_ability_layout.columns.x[2] -= 8;
            } else {
                menu_copy_u8_to_u16(
                    g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.turns, count);
                window_x = 0x94;
                row_mode = 5;
                g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
                g_menu_ability_layout.columns.x[1] -= 4;
                g_menu_ability_layout.columns.x[2] -= 4;
            }
        }
        row_flags = TEXT_ID_ABILITY_NAME_BASE;
        columns = 5;
    }

    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].inner_width = window_x;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.w = window_x;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].text_image.width = window_x;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].header_id = row_mode;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].select_text_table = columns;
    for (i = 0; i < count; i++) {
        g_ai_workspace_ptr->ability_list.ids[i] |= row_flags;
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            if (unit->mp - g_ai_workspace_ptr->ability_list.mp_costs[i] < 0
                && g_ai_workspace_ptr->ability_list.skill_ct[i] != 0) {
                g_menu_ability_display_flags[i] = 8;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT || type == ACTION_MENU_TYPE_MONSTER) {
            if ((g_ai_workspace_ptr->ability_list.flags[i] & 2) && g_ai_workspace_ptr->ability_list.skill_ct[i] != 0
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)]
                    & STATUS_BYTE_MASK(STATUS_ID_SILENCE))) {
                g_menu_ability_display_flags[i] = 8;
            }
        }
    }
    if (count == 0) {
        count = 1;
        g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].header_id = 0;
        g_ai_workspace_ptr->ability_list.ids[0] = TEXT_ID_ABILITY_NAME_BASE;
        g_ai_workspace_ptr->ability_list.values[0] = 0;
        g_ai_workspace_ptr->ability_list.extras[0] = 0;
        g_menu_ability_display_flags[0] = 4;
    }
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.y = 0x30;
    if (count >= 7) {
        g_menu_ability_layout.columns.hidden_rows = count - 6;
        g_menu_ability_layout.columns.row_count = 6;
    } else {
        g_menu_ability_layout.columns.hidden_rows = 0;
        g_menu_ability_layout.columns.row_count = count;
        g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.y += (6 - count) * 8;
    }
    /* The menu thread's redraw, cursor and refresh parameters. */
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1;
    g_thread_contexts[g_thread_current_id].function_parameter_3 = 0;
    g_thread_contexts[g_thread_current_id].function_parameter_4 = 1;
}

/* Menu entry 4's value and the loop counter share $s0 in the target: the
 * index dies after the two table loads and the register is reused by the
 * `sll s0,v0,0x1` that forms the counter, so they are one variable here.
 * g_menu_action_slot_row_actions is written as an in-loop expression rather than a pre-loop
 * pointer local so that loop.c hoists it into the preheader after the bltz,
 * which is where the target's `lui/addiu s8` pair sits. */

/*
 * Walk the stored unit's
 * menu record slots from the top value down to zero, presenting each through
 * the inner subroutine and writing the chosen entry back into the record.
 */
void menu_select_unit_action_slots_thread(void) {
    s32 i = g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index;
    s32 menu = g_action_menu_row_types[i];
    s32 check_unit = g_battle_menu_skillset_disabled_flags[i];
    menu_unit_selection_record_t* row = &g_menu_unit_selection_records[g_menu_selected_unit.battle_id];
    menu_list_selection_t* slot;
    u8* pair;
    s32 j;
    s16 text;

    /* Typed list indexing changes the address order; these byte views retain it. */
    for (i = (g_menu_restore_pending == 0) ? 2 : 0; i >= 0; i--) {
        menu_check_action_slot_restrictions(menu, i, check_unit);
        if (menu == ACTION_MENU_TYPE_ARITHMETICKS) {
            slot = (menu_list_selection_t*)((u8*)row + (i + offsetof(menu_unit_selection_record_t, lists[4])) + i);
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index = slot->selected_index;
            g_menu_ability_layout.row_offset = slot->scroll_offset;
        } else {
            pair = (u8*)row
                + g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index * sizeof(menu_list_selection_t);
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index
                = pair[offsetof(menu_unit_selection_record_t, lists[0].selected_index)];
            g_menu_ability_layout.row_offset = pair[offsetof(menu_unit_selection_record_t, lists[0].scroll_offset)];
            slot = (menu_list_selection_t*)(pair + offsetof(menu_unit_selection_record_t, lists));
        }
        for (j = 0; j < 6; j++) {
            if (menu != ACTION_MENU_TYPE_ARITHMETICKS || i == 0) {
                g_menu_action_slot_row_actions[j] = 0x1024;
            } else {
                g_menu_action_slot_row_actions[j] = -1;
            }
        }
        if (g_action_menu_row_types[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index]
            == ACTION_MENU_TYPE_CHANGE_EQUIPMENT) {
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index = 0;
            menu_run_equip_thread();
        }
        g_thread_call_target = menu_build_ability_list;
        thread_call_on_main_stack(i);
        script_pulse_tutorial_wait_value(0xFB);
        menu_run_scrolling_ability_list_thread_variant();
        slot->selected_index = g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index;
        slot->scroll_offset = g_menu_ability_layout.row_offset;
        if (menu != ACTION_MENU_TYPE_ARITHMETICKS) {
            thread_exit_current();
        }
        if (g_menu_action_slot_selected_option == -1) {
            if (i == 2) {
                thread_exit_current();
            }
            i += 2;
        } else {
            text = g_ai_workspace_ptr->ability_list
                       .ids[g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index]
                - 0x7000;
            if (i == 2) {
                g_menu_selected_action.calculator_type = text;
            }
            if (i == 1) {
                g_menu_selected_action.calculator_multiplier = text;
            }
            if (i == 0) {
                g_menu_selected_action.ability_id = text;
            }
        }
    }
    thread_exit_current();
}

/* Build skillset selection rows and size the selected unit's menu. */
void menu_build_skillset_entries(void) {
    menu_text_row_t* rows;
    s32 count;
    s32 i;
    s16 height;
    u8 separator;

    count = 0;
    g_menu_selected_skill_ct = 0;
    g_menu_skillsets[0] = 0;
    g_menu_skillset_build_result = battle_menu_build_unit_action_menus(g_menu_selected_unit.battle_id, g_menu_skillsets,
        g_battle_menu_skillset_disabled_flags, g_action_menu_row_types);
    rows = (menu_text_row_t*)text_find_entry(0x5010);
    if (battle_unit_get_action_restriction(menu_get_selected_unit_stats())
        == BATTLE_UNIT_ACTION_RESTRICTION_DEEP_WATER) {
        for (i = 0; i < 6; i++) {
            g_battle_menu_skillset_disabled_flags[i] = 1;
            if (g_action_menu_row_types[i] == ACTION_MENU_TYPE_UNKNOWN_0D) {
                g_battle_menu_skillset_disabled_flags[i] = 0;
            }
        }
    }
    /* The separator byte is materialised before the row counter in the
     * target; a hoisted constant lands after the counter's initializer. */
    separator = 0xfa;
    for (i = 5; i >= 0; i--) {
        rows[i].terminator = separator;
    }
    for (i = 0; i < 6; i++) {
        rows[i].terminator = 0xfa;
        if (g_menu_skillsets[i] == SKILLSET_ID_NONE) {
            break;
        }
        if (g_battle_menu_skillset_disabled_flags[i] != 0) {
            rows[i].style = 4;
            g_menu_skillset_row_actions[i] = 0x1003;
        } else {
            rows[i].style = 0;
            g_menu_skillset_row_actions[i] = 3;
        }
        if (g_menu_skillset_build_result != 0) {
            g_text_substitution_values[count] = TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
        } else {
            g_text_substitution_values[count] = g_menu_skillsets[i] + TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
        }
        count++;
    }
    if (script_get_variable(EVENT_SCRIPT_VAR_ANYTHING_ACTION_ENABLED) != 0 && i < 6) {
        rows[i].style = 0;
        g_menu_skillset_row_actions[i] = 0x16;
        g_menu_skillsets[i] = ACTION_MENU_PSEUDO_SKILLSET_ANYTHING;
        g_menu_skillsets[i + 1] = SKILLSET_ID_NONE;
        g_text_substitution_values[count++] = TEXT_ID_SKILLSET_NAME_SOURCE_BASE + ACTION_MENU_PSEUDO_SKILLSET_ANYTHING;
    }
    height = count * 16 + 16;
    rows[count - 1].terminator = 0xff;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].max_row_index = count - 1;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].text_image.height = height;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].window.h = height;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].inner_height = height;
}

/* Build the skillset list, then wait for its submenu or run icon selection.
 * The completion value written to the thread's
 * second parameter is materialised before the branch, which puts `li v1,1`
 * in the beqz delay slot ahead of the 0xFC argument. */
void menu_run_skillset_thread(void) {
    s32 param_value;
    menu_unit_selection_record_t* row;
    s32* param;

    menu_check_action_restrictions();
    g_thread_call_target = menu_build_skillset_entries;
    thread_call_on_main_stack();
    param_value = 1;
    if (g_menu_skillset_build_result != 0) {
        g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index = 0;
        if (g_menu_restore_pending == 0) {
            thread_start(g_thread_current_id - 1, menu_select_unit_action_slots_thread);
            thread_set_parameters(
                g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST], 0, 0);
        } else {
            thread_wait_frames(2);
        }
        for (;;) {
            thread_yield();
            if (thread_is_running_asm_variant(g_thread_current_id - 1) == 0) {
                thread_exit_current();
            }
        }
    }
    param = &g_thread_contexts[g_thread_current_id].function_parameter_2;
    *param = param_value;
    row = &g_menu_unit_selection_records[g_menu_selected_unit.battle_id];
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index = row->skillset_index;
    g_menu_restore_depth = 1;
    script_pulse_tutorial_wait_value(0xFC);
    menu_run_icon_selection_loop();
    row->skillset_index = g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index;
    thread_exit_current();
}

/* Record the current menu's selection byte for the selected unit, when that
 * menu appears in the tracked menu-id table. */
void menu_record_selection_for_selected_unit(void) {
    menu_unit_selection_record_t* records;
    s16* selected;
    s32 i;

    selected = &g_menu_selected_unit.battle_id;
    records = g_menu_unit_selection_records;
    for (i = 0; i < 12; i++) {
        if (g_menu_current_id == g_menu_command_maps[i].menu_id) {
            records[*selected].command_index = g_menu_thread_menu_data[g_menu_current_id].selected_index;
        }
    }
}

/* Thread entry of the turn menus (entries 0x00, 0x0e, 0x13, 0x14, 0x20, 0x21 and
 * 0x2e-0x33 of the idle action menu table): run the icon selection loop, then
 * remember the chosen row for the active-turn unit and exit. */
void menu_build_main(void) {
    menu_entry_t* parameter;
    menu_unit_selection_record_t* record;
    s32 unit_id;

    parameter = (menu_entry_t*)thread_get_current_parameter_1_variant();
    unit_id = g_menu_selected_unit.battle_id;
    record = &g_menu_unit_selection_records[unit_id];
    /* A nonzero second parameter makes the icon loop return here instead of exiting. */
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1;
    menu_run_icon_selection_loop();
    record->command_index = parameter->selected_index;
    thread_exit_current();
}
