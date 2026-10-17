/* Remembered command selections, Act and ability lists, and the turn-menu threads. */

#include "fft/ability_list.h"
#include "fft/ai.h"
#include "fft/ai_command.h"
#include "fft/command.h"
#include "fft/equipment.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/menu_selection.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/types.h"

/* The selected row and scroll position of an ability list. */
typedef struct menu_list_selection {
    u8 selected_index;
    u8 scroll_offset;
} menu_list_selection_t;
typedef char menu_list_selection_size_must_be_2[sizeof(menu_list_selection_t) == 2 ? 1 : -1];

/* One remembered command/skillset selection per unit, followed by seven
 * ability-list positions. Arithmeticks uses list positions 4, 5 and 6. */
typedef struct menu_unit_selection_record {
    u8 command_index;               /* 0x00 */
    u8 _unknown_01;                 /* 0x01; reset to 0xff */
    u8 skillset_index;              /* 0x02 */
    menu_list_selection_t lists[7]; /* 0x03 */
} menu_unit_selection_record_t;
typedef char menu_unit_selection_record_size_must_be_17[(sizeof(menu_unit_selection_record_t) == 17) ? 1 : -1];

/* Remap three remembered selections to one replacement for a menu. */
typedef struct menu_selection_remap {
    u8 menu_id;
    u8 replacement;
    u8 matches[3];
} menu_selection_remap_t;
typedef char menu_selection_remap_size_must_be_5[sizeof(menu_selection_remap_t) == 5 ? 1 : -1];

extern menu_unit_selection_record_t g_menu_unit_selection_records[];
extern menu_selection_remap_t g_menu_selection_remaps[8];

/* Provisional: one 5-byte row of a menu text entry as rewritten by command_build_act_menu: the style byte at +1 is 4
 * for a grayed row, the byte at +3 is the row separator (0xfa) or the list terminator (0xff). */
typedef struct menu_text_row {
    u8 _unused_00;
    u8 style; /* 0x01 */
    u8 _unused_02;
    u8 terminator; /* 0x03: 0xfa continues, 0xff ends */
    u8 _unused_04;
} menu_text_row_t;

static u8 g_menu_cleared_entry_indices[11] = { MENU_COMMAND_ID_WAIT_YES_NO, MENU_COMMAND_ID_PROMPT_YES_NO,
    MENU_COMMAND_ID_MESSAGE_YES_NO, MENU_COMMAND_ID_TARGET_KIND, MENU_COMMAND_ID_PANEL_EXECUTE,
    MENU_COMMAND_ID_MESSAGE_TARGET_KIND, MENU_COMMAND_ID_MESSAGE_PANEL_EXECUTE, MENU_COMMAND_ID_AUTO_BATTLE_UNIT,
    MENU_COMMAND_ID_MESSAGE_ATTACK_UNIT, MENU_COMMAND_ID_MESSAGE_DEFEND_UNIT, MENU_COMMAND_ID_MESSAGE_EXECUTE };

/* Reset the cursor row of the 11 menu entries listed in g_menu_cleared_entry_indices. */
void menu_clear_entry_selections(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        g_menu_thread_menu_data[g_menu_cleared_entry_indices[i]].selected_index = 0;
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_clear_entry_selections and command_restore_unit_selection.
 * Nothing on the disc calls it or points at it; the name is a neutral
 * placeholder until a reference turns up. */
void menu_noop_7(void) {
}

/* Restore the selected unit's remembered command row, applying menu remaps. */
void command_restore_unit_selection(s32 menu_id) {
    menu_unit_selection_record_t* record;
    s32 i;
    s32 row;
    s32 candidate;
    menu_selection_remap_t* remap;

    for (i = 0; i < 12; i++) {
        if (g_command_maps[i].menu_id == menu_id) {
            break;
        }
    }
    if (i == 12) {
        return;
    }
    record = &g_menu_unit_selection_records[g_unit_panel_selected_unit.battle_id];
    for (row = 0, remap = g_menu_selection_remaps; row < 8; remap++, row++) {
        if (menu_id == g_menu_selection_remaps[row].menu_id) {
            /* Keep the signed byte-table index used by the target. */
            for (candidate = 2; candidate < 5; candidate++) {
                if (record->command_index == remap->matches[candidate - 2]) {
                    record->command_index = g_menu_selection_remaps[row].replacement;
                    break;
                }
            }
        }
    }
    g_menu_thread_menu_data[menu_id].selected_index = record->command_index;
}

enum { MENU_UNIT_SELECTION_RECORD_BYTES = (s32)sizeof(menu_unit_selection_record_t) };

/* Reset the per-unit menu records, clearing each and marking byte 1 as
 * unset (0xFF).
 *
 * One row per battle unit, each 17 bytes. A typed row-array version changes the target's
 * induction variables (84 bytes rather than 76), so retain the byte view.
 * Use an integer countdown: decrementing an element pointer after row[0]
 * would form a pointer before the first row. */
void command_reset_unit_records(void) {
    s32 index;
    u8* row;
    s32 count;
    u8 fill = 0xFF;

    index = 0;
    row = (u8*)g_menu_unit_selection_records;
    do {
        count = MENU_UNIT_SELECTION_RECORD_BYTES - 1;
        do {
            row[count] = 0;
            count -= 1;
        } while (count >= 0);
        ((u8*)g_menu_unit_selection_records)[index + 1] = fill;
        index += MENU_UNIT_SELECTION_RECORD_BYTES;
        row += MENU_UNIT_SELECTION_RECORD_BYTES;
    } while (index < UNIT_SLOT_COUNT * MENU_UNIT_SELECTION_RECORD_BYTES);
}

/* Clear one unit's remembered menu rows, as command_reset_unit_records does for all (byte 1 back to
 * 0xff), but only for a unit under manual control. The test sits inside the byte loop. */
void command_reset_unit_record(s32 battle_id) {
    unit_stats_t* stats = unit_get_stats(battle_id);
    menu_unit_selection_record_t* records = g_menu_unit_selection_records;
    s32 i;

    for (i = 0; i < MENU_UNIT_SELECTION_RECORD_BYTES; i++) {
        if (stats->auto_battle_setting == AI_AUTOBATTLE_MANUAL) {
            ((u8*)&records[battle_id])[i] = 0;
            g_menu_unit_selection_records[battle_id]._unknown_01 = 0xFF;
        }
    }
}

/* The command record the menus fill when the player picks a command (g_command_selected_command). */
ai_command_t* command_get_selected(void) {
    return &g_command_selected_command;
}

/* Post the close command after two frames under the mini-menu task id. */
void mini_menu_cancel_delay_thread(void) {
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    thread_wait_frames(2);
    g_command_pending_selection[0] = 7;
    thread_exit_current();
}

/* Queue the confirm sound and hold the mini-menu task id for 16 frames. */
void mini_menu_confirm_delay_thread(void) {
    g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    thread_wait_frames(16);
    thread_exit_current();
}

/* Widen bytes into 16-bit menu values up to a 0xff terminator, which is not copied, and return the
 * count. */
static s32 menu_copy_u8_to_u16_terminated(u16* destination, const u8* source) {
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
static void menu_copy_u8_to_u16(u16* destination, const u8* source, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        *destination = *source;
        source++;
        destination++;
    }
}

/* Builds the ability list for the active skillset menu entry (menu entry 4's
 * selected_index selects the skillset; g_menu_skillset_types gives its list type). Each type
 * fills the 0x52-row work buffer at *g_ai_workspace_ptr through its own loader, then
 * the rows are sized, grayed (not enough MP, or silenced for voice abilities)
 * and g_ability_list_ability_layout is set up for 6 visible rows.
 *
 * Matching notes: the layout block is one struct (its x/y are s16, so `-= 4`
 * stays an addiu immediate and the constant is not shared); window_x is a
 * separate local from the unit index, and its updates follow the loader
 * calls in each arm. */
static void command_build_ability_list(s32 mode) {
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
    menu_ability_list_t* list;

    g_ability_list_ability_layout.columns.text_ids[0] = (s16*)g_ai_workspace_ptr->ability_list.ids;
    g_ability_list_ability_layout.columns.text_ids[1] = (s16*)g_ai_workspace_ptr->ability_list.values;
    g_ability_list_ability_layout.columns.text_ids[2] = (s16*)g_ai_workspace_ptr->ability_list.extras;

    unit_index = g_unit_panel_selected_unit.battle_id;
    unit = menu_get_selected_unit_stats();
    type = g_menu_skillset_types[g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].selected_index];
    skillset = g_command_skillsets[g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].selected_index];
    g_command_selected_command.data.action.skillset_id = skillset;

    for (i = 0; i < 80; i++) {
        g_ai_workspace_ptr->ability_list.ids[i] = 0xFFFF;
        g_ai_workspace_ptr->ability_list.mp_costs[i] = 0xFF;
        g_ai_workspace_ptr->ability_list.ct[i] = 1;
        g_ai_workspace_ptr->ability_list.values[i] = 0;
        g_ai_workspace_ptr->ability_list.extras[i] = 0xFFFF;
        g_ai_workspace_ptr->ability_list.flags[i] = 0;
        g_ai_workspace_ptr->ability_list.turns[i] = 1;
        g_ai_workspace_ptr->ability_list.skill_ct[i] = 0xFF;
        g_ability_list_ability_display_flags[i] = 0;
    }
    /* Row 0x50 ends the byte columns with the 0xff terminator the widening copies stop at. */
    g_ai_workspace_ptr->ability_list.mp_costs[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.ct[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.flags[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.turns[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.skill_ct[0x50] = 0xFF;

    if (type == ACTION_MENU_TYPE_DEFAULT) {
        list = &g_ai_workspace_ptr->ability_list;
        skillset_get_unit_ability_data(
            unit_index, skillset, (s16*)list->ids, list->mp_costs, list->ct, 0, list->flags, list->turns);
        mem_copy_bytes_nonempty(g_ai_workspace_ptr->ability_list.skill_ct, g_ai_workspace_ptr->ability_list.ct, 0x50);
    }
    if (type == ACTION_MENU_TYPE_ITEM_INVENTORY) {
        skillset_load_item_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        skillset_load_throw_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_ARITHMETICKS) {
        if (mode == MENU_ARITHMETICKS_LIST_ATTRIBUTES) {
            skillset_math_load_attributes(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
        if (mode == MENU_ARITHMETICKS_LIST_MULTIPLES) {
            skillset_math_load_multiples(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
        if (mode == MENU_ARITHMETICKS_LIST_ABILITIES) {
            skillset_math_collect_calculator_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
    }
    if (type == ACTION_MENU_TYPE_MONSTER) {
        skillset_monster_collect_abilities(
            unit_index, skillset, g_ai_workspace_ptr->ability_list.ids, 0, g_ai_workspace_ptr->ability_list.flags);
    }
    if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        skillset_load_draw_out_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_CHARGE) {
        skillset_load_charge_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids,
            g_ai_workspace_ptr->ability_list.mp_costs, g_ai_workspace_ptr->ability_list.turns);
        mem_copy_bytes_nonempty(
            g_ai_workspace_ptr->ability_list.skill_ct, g_ai_workspace_ptr->ability_list.mp_costs, 0x50);
    }

    window_x = 0xAC;
    if (type != ACTION_MENU_TYPE_ARITHMETICKS) {
        mode = MENU_ARITHMETICKS_LIST_ABILITIES;
    }

    g_ability_list_ability_layout.columns.mode[1] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_ability_list_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_ability_list_ability_layout.columns.x[1] = 0x6A;
    g_ability_list_ability_layout.columns.x[2] = 0x7E;
    g_ability_list_ability_layout.columns.mode[0] = MENU_TEXT_COLUMN_TEXT;
    if (g_command_action_type_modes[type][1] == 1) { /* lists by id */
        /* Inventory loaders leave item ids in the byte column mp_costs; the value column
         * shows each item's quantity. */
        count = menu_copy_u8_to_u16_terminated(
            g_ai_workspace_ptr->ability_list.ids, g_ai_workspace_ptr->ability_list.mp_costs);
        menu_copy_u8_to_u16(g_ai_workspace_ptr->ability_list.extras, g_ai_workspace_ptr->ability_list.turns, count);
        found = 0;
        for (i = 0; i < count; i++) {
            g_ai_workspace_ptr->ability_list.values[i] = g_item_quantities[g_ai_workspace_ptr->ability_list.ids[i]];
            if (g_ai_workspace_ptr->ability_list.values[i] == 0) {
                g_ability_list_ability_display_flags[i] = 4;
            } else {
                found = 1;
            }
        }

        /* With nothing in stock the third column hides and the window narrows. */

        row_mode = 7;
        if (found != 0) {
            window_x -= 4;
        } else {
            g_ability_list_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
            menu_copy_u8_to_u16(g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.turns, count);
            window_x = 0x94;
            row_mode = 8;
        }

        row_flags = TEXT_ID_ITEM_NAME_BASE;
        g_ability_list_ability_layout.columns.x[1] -= 4;
        g_ability_list_ability_layout.columns.x[2] -= 4;
        columns = 0x11;
        if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
            columns = 0x17;
        }
    } else {
        found = 0;
        for (count = 0; count < 64; count++) {
            if (g_ai_workspace_ptr->ability_list.ids[count] == 0xFFFF) {
                break;
            }
        }

        /* The MP column shows only when some ability costs MP. */
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
            g_ability_list_ability_layout.columns.mode[1] = MENU_TEXT_COLUMN_HIDDEN;
            g_ability_list_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
            row_mode = 9;
        } else {
            if (found != 0 && type == ACTION_MENU_TYPE_DEFAULT) {
                menu_copy_u8_to_u16_terminated(
                    g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.mp_costs);
                window_x -= 8;
                menu_copy_u8_to_u16(
                    g_ai_workspace_ptr->ability_list.extras, g_ai_workspace_ptr->ability_list.turns, count);
                row_mode = 6;
                g_ability_list_ability_layout.columns.x[1] -= 8;
                g_ability_list_ability_layout.columns.x[2] -= 8;
            } else {
                menu_copy_u8_to_u16(
                    g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.turns, count);
                window_x = 0x94;
                row_mode = 5;
                g_ability_list_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
                g_ability_list_ability_layout.columns.x[1] -= 4;
                g_ability_list_ability_layout.columns.x[2] -= 4;
            }
        }

        row_flags = TEXT_ID_ABILITY_NAME_BASE;
        columns = 5;
    }

    g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].inner_width = window_x;
    g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].window.w = window_x;
    g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].text_image.source.w = window_x;
    g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].header_id = row_mode;
    g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].select_text_table = columns;

    for (i = 0; i < count; i++) {
        g_ai_workspace_ptr->ability_list.ids[i] |= row_flags;
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            if (unit->mp - g_ai_workspace_ptr->ability_list.mp_costs[i] < 0
                && g_ai_workspace_ptr->ability_list.skill_ct[i] != 0) {
                g_ability_list_ability_display_flags[i] = 8;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT || type == ACTION_MENU_TYPE_MONSTER) {
            if ((g_ai_workspace_ptr->ability_list.flags[i] & MENU_ABILITY_FLAG_AFFECTED_BY_SILENCE)
                && g_ai_workspace_ptr->ability_list.skill_ct[i] != 0
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)]
                    & STATUS_BYTE_MASK(STATUS_ID_SILENCE))) {
                g_ability_list_ability_display_flags[i] = 8;
            }
        }
    }

    /* An empty list shows one disabled row for ability 0. */
    if (count == 0) {
        count = 1;
        g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].header_id = 0;
        g_ai_workspace_ptr->ability_list.ids[0] = TEXT_ID_ABILITY_NAME_BASE;
        g_ai_workspace_ptr->ability_list.values[0] = 0;
        g_ai_workspace_ptr->ability_list.extras[0] = 0;
        g_ability_list_ability_display_flags[0] = 4;
    }

    /* Six rows show at most; a shorter list's window moves down 8 pixels per missing row. */
    g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].window.y = 0x30;
    if (count >= 7) {
        g_ability_list_ability_layout.columns.hidden_rows = count - 6;
        g_ability_list_ability_layout.columns.row_count = 6;
    } else {
        g_ability_list_ability_layout.columns.hidden_rows = 0;
        g_ability_list_ability_layout.columns.row_count = count;
        g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].window.y += (6 - count) * 8;
    }

    /* The menu thread's redraw, cursor and refresh parameters. */
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1;
    g_thread_contexts[g_thread_current_id].function_parameter_3 = 0;
    g_thread_contexts[g_thread_current_id].function_parameter_4 = 1;
}

/*
 * Run the ability list of the chosen skillset row, remembering its cursor row and scroll in
 * the selected unit's menu record. Arithmeticks walks three lists, attribute (i = 2), multiple
 * (1) and ability (0), storing each choice in the selected action; backing out of one returns
 * to the previous list. Other skillsets run a single list and exit.
 *
 * Menu entry 4's value and the loop counter share $s0 in the target: the
 * index dies after the two table loads and the register is reused by the
 * `sll s0,v0,0x1` that forms the counter, so they are one variable here.
 * g_command_action_slot_row_actions is written as an in-loop expression rather than a pre-loop
 * pointer local so that loop.c hoists it into the preheader after the bltz,
 * which is where the target's `lui/addiu s8` pair sits.
 */
void command_ability_list_thread(void) {
    s32 i = g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].selected_index;
    s32 menu = g_menu_skillset_types[i];
    s32 check_unit = g_command_skillset_disabled_flags[i];
    menu_unit_selection_record_t* row = &g_menu_unit_selection_records[g_unit_panel_selected_unit.battle_id];
    menu_list_selection_t* slot;
    u8* pair;
    s32 j;
    s16 text;

    /* Typed list indexing changes the address order; these byte views retain it. */
    for (i = (g_menu_restore_pending == 0) ? 2 : 0; i >= 0; i--) {
        command_check_row_restrictions(menu, i, check_unit);
        if (menu == ACTION_MENU_TYPE_ARITHMETICKS) {
            slot = (menu_list_selection_t*)((u8*)row + (i + offsetof(menu_unit_selection_record_t, lists[4])) + i);
            g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].selected_index = slot->selected_index;
            g_ability_list_ability_layout.row_offset = slot->scroll_offset;
        } else {
            pair = (u8*)row
                + g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].selected_index * sizeof(menu_list_selection_t);
            g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].selected_index
                = pair[offsetof(menu_unit_selection_record_t, lists[0].selected_index)];
            g_ability_list_ability_layout.row_offset
                = pair[offsetof(menu_unit_selection_record_t, lists[0].scroll_offset)];
            slot = (menu_list_selection_t*)(pair + offsetof(menu_unit_selection_record_t, lists));
        }
        for (j = 0; j < 6; j++) {
            if (menu != ACTION_MENU_TYPE_ARITHMETICKS || i == 0) {
                g_command_action_slot_row_actions[j] = MENU_ROW_ACTION_SILENT | MENU_COMMAND_ID_ABILITY_CONFIRM;
            } else {
                g_command_action_slot_row_actions[j] = -1;
            }
        }
        if (g_menu_skillset_types[g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].selected_index]
            == ACTION_MENU_TYPE_CHANGE_EQUIPMENT) {
            g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].selected_index = 0;
            equipment_thread();
        }
        g_thread_call_target = command_build_ability_list;
        thread_call_on_main_stack(i);
        tutorial_pulse_wait_value(SCRIPT_TUTORIAL_WAIT_ABILITY_LIST);
        ability_list_scrolling_thread();
        slot->selected_index = g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].selected_index;
        slot->scroll_offset = g_ability_list_ability_layout.row_offset;
        if (menu != ACTION_MENU_TYPE_ARITHMETICKS) {
            thread_exit_current();
        }
        if (g_command_action_slot_selected_option == -1) {
            if (i == 2) {
                thread_exit_current();
            }
            i += 2;
        } else {
            text = g_ai_workspace_ptr->ability_list
                       .ids[g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].selected_index]
                - 0x7000;
            if (i == 2) {
                g_command_selected_command.data.action.calculator_type = text;
            }
            if (i == 1) {
                g_command_selected_command.data.action.calculator_multiplier = text;
            }
            if (i == 0) {
                g_command_selected_command.data.action.ability_id = text;
            }
        }
    }
    thread_exit_current();
}

/* Build the Act submenu (SKILLSET_LIST) for the selected unit: its skillset rows from
 * skillset_build_menu_list, every row but type 0x0D disabled in deep water, disabled
 * rows grayed and opening the ability list silently, an Anything row when that script variable is
 * set, and the entry sized to the rows. */
static void command_build_act_menu(void) {
    menu_text_row_t* rows;
    s32 count = 0;
    s32 i;
    s16 height;
    u8 separator;

    g_command_selected_skill_ct = 0;
    g_command_skillsets[0] = 0;
    g_command_skillset_build_result = skillset_build_menu_list(g_unit_panel_selected_unit.battle_id,
        g_command_skillsets, g_command_skillset_disabled_flags, g_menu_skillset_types);
    rows = (menu_text_row_t*)text_find_entry(0x5010);
    if (unit_get_action_restriction(menu_get_selected_unit_stats()) == UNIT_ACTION_RESTRICTION_DEEP_WATER) {
        for (i = 0; i < 6; i++) {
            g_command_skillset_disabled_flags[i] = 1;
            if (g_menu_skillset_types[i] == ACTION_MENU_TYPE_UNKNOWN_0D) {
                g_command_skillset_disabled_flags[i] = 0;
            }
        }
    }
    /* The separator byte is materialized before the row counter in the
     * target; a hoisted constant lands after the counter's initializer. */
    separator = 0xfa;
    for (i = 5; i >= 0; i--) {
        rows[i].terminator = separator;
    }
    for (i = 0; i < 6; i++) {
        rows[i].terminator = 0xfa;
        if (g_command_skillsets[i] == SKILLSET_ID_NONE) {
            break;
        }
        if (g_command_skillset_disabled_flags[i] != 0) {
            rows[i].style = 4;
            g_command_skillset_row_actions[i] = MENU_ROW_ACTION_SILENT | MENU_COMMAND_ID_ABILITY_LIST;
        } else {
            rows[i].style = 0;
            g_command_skillset_row_actions[i] = MENU_COMMAND_ID_ABILITY_LIST;
        }
        if (g_command_skillset_build_result != 0) {
            g_text_substitution_values[count] = TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
        } else {
            g_text_substitution_values[count] = g_command_skillsets[i] + TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
        }
        count++;
    }
    if (script_get_variable(EVENT_SCRIPT_VAR_ANYTHING_ACTION_ENABLED) != 0 && i < 6) {
        rows[i].style = 0;
        g_command_skillset_row_actions[i] = 0x16;
        g_command_skillsets[i] = ACTION_MENU_PSEUDO_SKILLSET_ANYTHING;
        g_command_skillsets[i + 1] = SKILLSET_ID_NONE;
        g_text_substitution_values[count++] = TEXT_ID_SKILLSET_NAME_SOURCE_BASE + ACTION_MENU_PSEUDO_SKILLSET_ANYTHING;
    }
    height = count * 16 + 16;
    rows[count - 1].terminator = 0xff;
    g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].max_row_index = count - 1;
    g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].text_image.source.h = height;
    g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].window.h = height;
    g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].inner_height = height;
}

/* Build the skillset list, then wait for its submenu or run icon selection.
 * The completion value written to the thread's
 * second parameter is materialized before the branch, which puts `li v1,1`
 * in the beqz delay slot ahead of the SCRIPT_TUTORIAL_WAIT_SKILLSET_LIST argument. */
void command_act_menu_thread(void) {
    s32 param_value;
    menu_unit_selection_record_t* row;
    s32* param;

    command_check_restrictions();
    g_thread_call_target = command_build_act_menu;
    thread_call_on_main_stack();
    param_value = 1;
    if (g_command_skillset_build_result != 0) {
        g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].selected_index = 0;
        if (g_menu_restore_pending == 0) {
            thread_start(g_thread_current_id - 1, command_ability_list_thread);
            thread_set_parameters(
                g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST], 0, 0);
        } else {
            thread_wait_frames(2);
        }
        for (;;) {
            thread_yield();
            if (thread_is_running_asm(g_thread_current_id - 1) == 0) {
                thread_exit_current();
            }
        }
    }
    param = &g_thread_contexts[g_thread_current_id].function_parameter_2;
    *param = param_value;
    row = &g_menu_unit_selection_records[g_unit_panel_selected_unit.battle_id];
    g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].selected_index = row->skillset_index;
    g_menu_restore_depth = 1;
    tutorial_pulse_wait_value(SCRIPT_TUTORIAL_WAIT_SKILLSET_LIST);
    menu_run_icon_selection_loop();
    row->skillset_index = g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].selected_index;
    thread_exit_current();
}

/* Record the current menu's selection byte for the selected unit, when that
 * menu appears in the tracked menu-id table. */
void command_record_unit_selection(void) {
    menu_unit_selection_record_t* records;
    s16* selected = &g_unit_panel_selected_unit.battle_id;
    s32 i;

    records = g_menu_unit_selection_records;
    for (i = 0; i < 12; i++) {
        if (g_command_current_id == g_command_maps[i].menu_id) {
            records[*selected].command_index = g_menu_thread_menu_data[g_command_current_id].selected_index;
        }
    }
}

/* Thread entry of the turn menus (entries 0x00, 0x0e, 0x13, 0x14, 0x20, 0x21 and
 * 0x2e-0x33 of the idle action menu table): run the icon selection loop, then
 * remember the chosen row for the active-turn unit and exit. */
void command_turn_menu_thread(void) {
    menu_entry_t* parameter = (menu_entry_t*)thread_get_current_parameter_1();
    menu_unit_selection_record_t* record;
    s32 unit_id = g_unit_panel_selected_unit.battle_id;

    record = &g_menu_unit_selection_records[unit_id];
    /* A nonzero second parameter makes the icon loop return here instead of exiting. */
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1;
    menu_run_icon_selection_loop();
    record->command_index = parameter->selected_index;
    thread_exit_current();
}
