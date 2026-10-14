#include "fft/memory.h"
/* Builds the ability list for the active skillset menu entry (menu entry 4's
 * selected_index selects the skillset; g_action_menu_row_types gives its list type). Each type
 * fills the 0x52-row work buffer at *g_ai_workspace_ptr through its own loader, then
 * the rows are sized, grayed (not enough MP, or silenced for voice abilities)
 * and the window layout at 0x80153c78 is set up for 6 visible rows.
 * WORLD twin of battle_menu_build_ability_list (0x8013fa6c); the two are masked-identical.
 *
 * Matching notes: the layout block is one struct (its values_x/extras_x are s16, so `-= 4`
 * stays an addiu immediate and the constant is not shared); window_x is a
 * separate local from the unit index, and its updates follow the loader
 * calls in each arm. The ff-terminated copy takes an ignored third argument
 * (0 or the row count), as the target's call sites load $a2. */
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_build_ability_list(s32 mode) {
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

    g_world_ability_menu_layout.ids = g_ai_workspace_ptr->ability_list.ids;
    g_world_ability_menu_layout.values = g_ai_workspace_ptr->ability_list.values;
    g_world_ability_menu_layout.extras = g_ai_workspace_ptr->ability_list.extras;
    unit_index = g_menu_selected_unit.battle_id;
    unit = world_unit_get_active_stats();
    type = g_action_menu_row_types[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index];
    skillset = g_menu_skillsets[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index];
    g_world_menu_preview_action.skillset_id = skillset;
    for (i = 0; i < 0x50; i++) {
        g_ai_workspace_ptr->ability_list.ids[i] = 0xFFFF;
        g_ai_workspace_ptr->ability_list.mp_costs[i] = 0xFF;
        g_ai_workspace_ptr->ability_list.ct[i] = 1;
        g_ai_workspace_ptr->ability_list.values[i] = 0;
        g_ai_workspace_ptr->ability_list.extras[i] = 0xFFFF;
        g_ai_workspace_ptr->ability_list.flags[i] = 0;
        g_ai_workspace_ptr->ability_list.turns[i] = 1;
        g_ai_workspace_ptr->ability_list.skill_ct[i] = 0xFF;
        g_world_menu_ability_display_flags[i] = 0;
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
            battle_menu_load_math_skill_attributes(unit_index, skillset, (s16*)g_ai_workspace_ptr->ability_list.ids);
        }
        if (mode == MENU_ARITHMETICKS_LIST_MULTIPLES) {
            battle_menu_load_math_skill_multiples(unit_index, skillset, (s16*)g_ai_workspace_ptr->ability_list.ids);
        }
        if (mode == MENU_ARITHMETICKS_LIST_ABILITIES) {
            battle_menu_collect_calculator_abilities(unit_index, skillset, (s16*)g_ai_workspace_ptr->ability_list.ids);
        }
    }
    if (type == ACTION_MENU_TYPE_MONSTER) {
        battle_menu_collect_monster_skill_abilities(unit_index, skillset, (s16*)g_ai_workspace_ptr->ability_list.ids, 0,
            g_ai_workspace_ptr->ability_list.flags);
    }
    if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        battle_menu_load_draw_out_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_CHARGE) {
        battle_menu_load_charge_skillset(unit_index, skillset, (s16*)g_ai_workspace_ptr->ability_list.ids,
            g_ai_workspace_ptr->ability_list.mp_costs, g_ai_workspace_ptr->ability_list.turns);
        mem_copy_bytes_nonempty(
            g_ai_workspace_ptr->ability_list.skill_ct, g_ai_workspace_ptr->ability_list.mp_costs, 0x50);
    }
    window_x = 0xAC;
    if (type != ACTION_MENU_TYPE_ARITHMETICKS) {
        mode = MENU_ARITHMETICKS_LIST_ABILITIES;
    }

    g_world_ability_menu_layout.values_mode = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_world_ability_menu_layout.extras_mode = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_world_ability_menu_layout.values_x = 0x6A;
    g_world_ability_menu_layout.extras_x = 0x7E;
    g_world_ability_menu_layout.ids_mode = MENU_TEXT_COLUMN_TEXT;
    if (g_world_menu_row_type_confirm_actions[type][1] == 1) {
        count = menu_copy_u8_to_u16_terminated(
            g_ai_workspace_ptr->ability_list.ids, g_ai_workspace_ptr->ability_list.mp_costs);
        menu_copy_u8_to_u16(g_ai_workspace_ptr->ability_list.extras, g_ai_workspace_ptr->ability_list.turns, count);
        found = 0;
        for (i = 0; i < count; i++) {
            g_ai_workspace_ptr->ability_list.values[i]
                = g_main_item_quantities[g_ai_workspace_ptr->ability_list.ids[i]];
            if (g_ai_workspace_ptr->ability_list.values[i] == 0) {
                g_world_menu_ability_display_flags[i] = 4;
            } else {
                found = 1;
            }
        }
        row_mode = 7;
        if (found != 0) {
            window_x -= 4;
        } else {
            g_world_ability_menu_layout.extras_mode = MENU_TEXT_COLUMN_HIDDEN;
            menu_copy_u8_to_u16(g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.turns, count);
            window_x = 0x94;
            row_mode = 8;
        }
        row_flags = TEXT_ID_ITEM_NAME_BASE;
        g_world_ability_menu_layout.values_x -= 4;
        g_world_ability_menu_layout.extras_x -= 4;
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
            g_world_ability_menu_layout.values_mode = MENU_TEXT_COLUMN_HIDDEN;
            g_world_ability_menu_layout.extras_mode = MENU_TEXT_COLUMN_HIDDEN;
            row_mode = 9;
        } else {
            if (found != 0 && type == ACTION_MENU_TYPE_DEFAULT) {
                menu_copy_u8_to_u16_terminated(
                    g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.mp_costs);
                window_x -= 8;
                menu_copy_u8_to_u16(
                    g_ai_workspace_ptr->ability_list.extras, g_ai_workspace_ptr->ability_list.turns, count);
                row_mode = 6;
                g_world_ability_menu_layout.values_x -= 8;
                g_world_ability_menu_layout.extras_x -= 8;
            } else {
                menu_copy_u8_to_u16(
                    g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.turns, count);
                window_x = 0x94;
                row_mode = 5;
                g_world_ability_menu_layout.extras_mode = MENU_TEXT_COLUMN_HIDDEN;
                g_world_ability_menu_layout.values_x -= 4;
                g_world_ability_menu_layout.extras_x -= 4;
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
                g_world_menu_ability_display_flags[i] = 8;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT || type == ACTION_MENU_TYPE_MONSTER) {
            if ((g_ai_workspace_ptr->ability_list.flags[i] & 2) && g_ai_workspace_ptr->ability_list.skill_ct[i] != 0
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)]
                    & STATUS_BYTE_MASK(STATUS_ID_SILENCE))) {
                g_world_menu_ability_display_flags[i] = 8;
            }
        }
    }
    if (count == 0) {
        count = 1;
        g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].header_id = 0;
        g_ai_workspace_ptr->ability_list.ids[0] = TEXT_ID_ABILITY_NAME_BASE;
        g_ai_workspace_ptr->ability_list.values[0] = 0;
        g_ai_workspace_ptr->ability_list.extras[0] = 0;
        g_world_menu_ability_display_flags[0] = 4;
    }
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.y = 0x30;
    if (count >= 7) {
        g_world_ability_menu_layout.hidden_rows = count - 6;
        g_world_ability_menu_layout.visible_rows = 6;
    } else {
        g_world_ability_menu_layout.hidden_rows = 0;
        g_world_ability_menu_layout.visible_rows = count;
        g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.y += (6 - count) * 8;
    }
    /* Menu-thread parameters 2-4 request a redraw, reset the cursor and
     * request a refresh. */
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1;
    g_thread_contexts[g_thread_current_id].function_parameter_3 = 0;
    g_thread_contexts[g_thread_current_id].function_parameter_4 = 1;
}
