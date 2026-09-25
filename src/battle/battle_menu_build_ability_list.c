/* Builds the ability list for the active skillset menu entry (menu entry 4's
 * selected_index selects the skillset; g_battle_action_menu_row_types gives its list type). Each type
 * fills the 0x52-row work buffer at *g_battle_ai_workspace_ptr through its own loader, then
 * the rows are sized, greyed (not enough MP, or silenced for voice abilities)
 * and the window layout at 0x80166994 is set up for 6 visible rows.
 * BATTLE twin of world_menu_build_ability_list (0x800f32e4); the two are masked-identical.
 *
 * Matching notes: the layout block is one struct (its x/y are s16, so `-= 4`
 * stays an addiu immediate and the constant is not shared); window_x is a
 * separate local from the unit index, and its updates follow the loader
 * calls in each arm. The ff-terminated copy takes an ignored third argument
 * (0 or the row count), as the target's call sites load $a2. */
#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Provisional ability-list work buffer at *g_battle_ai_workspace_ptr (0x52 rows each). */
typedef world_ability_list_t battle_menu_ability_list_t;

void battle_menu_build_ability_list(s32 mode) {
    s32 unit_index;
    s32 window_x;
    battle_stats_t* unit;
    s32 i;
    s32 type;
    s32 skillset;
    s32 count;
    s32 found;
    s32 columns;
    s32 row_mode;
    s32 row_flags;

    g_battle_ability_menu_layout.text_ids[0] = (s16*)((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids;
    g_battle_ability_menu_layout.text_ids[1] = (s16*)((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->values;
    g_battle_ability_menu_layout.text_ids[2] = (s16*)((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->extras;
    unit_index = g_battle_active_turn_unit.battle_id;
    unit = battle_unit_get_attacker_data_pointer();
    type = g_battle_action_menu_row_types[g_battle_menu_thread_menu_data[4].selected_index];
    skillset = g_battle_action_menu_skillsets[g_battle_menu_thread_menu_data[4].selected_index];
    g_battle_menu_selected_action.skillset = skillset;
    for (i = 0; i < 0x50; i++) {
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids[i] = 0xFFFF;
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs[i] = 0xFF;
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_23e[i] = 1;
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->values[i] = 0;
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->extras[i] = 0xFFFF;
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->flags[i] = 0;
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2[i] = 1;
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->enabled[i] = 0xFF;
        g_battle_menu_ability_display_flags[i] = 0;
    }
    ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs[0x50] = 0xFF;
    ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_23e[0x50] = 0xFF;
    ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->flags[0x50] = 0xFF;
    ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2[0x50] = 0xFF;
    ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->enabled[0x50] = 0xFF;

    if (type == ACTION_MENU_TYPE_DEFAULT) {
        battle_menu_get_unit_skillset_ability_data(unit_index, skillset,
            (s16*)((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_23e, 0,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->flags,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2);
        battle_copy_bytes(((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->enabled,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_23e, 0x50);
    }
    if (type == ACTION_MENU_TYPE_ITEM_INVENTORY) {
        battle_menu_display_item_inventory_ability(
            unit_index, skillset, ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs);
    }
    if (type == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        battle_menu_load_throw_abilities(
            unit_index, skillset, ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs);
    }
    if (type == ACTION_MENU_TYPE_ARITHMETICKS) {
        if (mode == 2) {
            battle_menu_load_math_skill_attributes(
                unit_index, skillset, ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids);
        }
        if (mode == 1) {
            battle_menu_load_math_skill_multiples(
                unit_index, skillset, ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids);
        }
        if (mode == 0) {
            battle_menu_collect_calculator_abilities(
                unit_index, skillset, ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids);
        }
    }
    if (type == ACTION_MENU_TYPE_MONSTER) {
        battle_menu_collect_monster_skill_abilities(unit_index, skillset,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids, 0,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->flags);
    }
    if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        battle_menu_load_draw_out_abilities(
            unit_index, skillset, ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs);
    }
    if (type == ACTION_MENU_TYPE_CHARGE) {
        battle_menu_load_charge_skillset(unit_index, skillset,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2);
        battle_copy_bytes(((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->enabled,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs, 0x50);
    }
    window_x = 0xAC;
    if (type != ACTION_MENU_TYPE_ARITHMETICKS) {
        mode = 0;
    }

    g_battle_ability_menu_layout.mode[1] = 3;
    g_battle_ability_menu_layout.mode[2] = 3;
    g_battle_ability_menu_layout.x[1] = 0x6A;
    g_battle_ability_menu_layout.x[2] = 0x7E;
    g_battle_ability_menu_layout.mode[0] = 0;
    if (g_battle_menu_action_type_modes[type][1] == 1) { /* lists by id */
        /* The target passes a third argument the two-parameter callee ignores. */
        count = ((s32 (*)(void*, u8*, s32))battle_menu_copy_ff_terminated_bytes_to_halfwords)(
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs, 0);
        battle_menu_widen_bytes_to_halfwords((s16*)((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->extras,
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2, count);
        found = 0;
        for (i = 0; i < count; i++) {
            ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->values[i]
                = g_main_item_quantities[((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids[i]];
            if (((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->values[i] == 0) {
                g_battle_menu_ability_display_flags[i] = 4;
            } else {
                found = 1;
            }
        }
        row_mode = 7;
        if (found != 0) {
            window_x -= 4;
        } else {
            g_battle_ability_menu_layout.mode[2] = 2;
            battle_menu_widen_bytes_to_halfwords((s16*)((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->values,
                ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2, count);
            window_x = 0x94;
            row_mode = 8;
        }
        row_flags = TEXT_ID_ITEM_NAME_BASE;
        g_battle_ability_menu_layout.x[1] -= 4;
        g_battle_ability_menu_layout.x[2] -= 4;
        columns = 0x11;
        if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
            columns = 0x17;
        }
    } else {
        found = 0;
        for (count = 0; count < 0x40; count++) {
            if (((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids[count] == 0xFFFF) {
                break;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            for (i = 0; i < count; i++) {
                if (((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs[i] != 0) {
                    found = 1;
                    break;
                }
            }
        }
        if (type == ACTION_MENU_TYPE_ARITHMETICKS && mode > 0) {
            window_x = 0x88;
            g_battle_ability_menu_layout.mode[1] = 2;
            g_battle_ability_menu_layout.mode[2] = 2;
            row_mode = 9;
        } else {
            if (found != 0 && type == ACTION_MENU_TYPE_DEFAULT) {
                /* The target passes a third argument the callee ignores. */
                ((s32 (*)(void*, u8*, s32))battle_menu_copy_ff_terminated_bytes_to_halfwords)(
                    ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->values,
                    ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs, count);
                window_x -= 8;
                battle_menu_widen_bytes_to_halfwords(
                    (s16*)((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->extras,
                    ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2, count);
                row_mode = 6;
                g_battle_ability_menu_layout.x[1] -= 8;
                g_battle_ability_menu_layout.x[2] -= 8;
            } else {
                battle_menu_widen_bytes_to_halfwords(
                    (s16*)((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->values,
                    ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2, count);
                window_x = 0x94;
                row_mode = 5;
                g_battle_ability_menu_layout.mode[2] = 2;
                g_battle_ability_menu_layout.x[1] -= 4;
                g_battle_ability_menu_layout.x[2] -= 4;
            }
        }
        row_flags = TEXT_ID_ABILITY_NAME_BASE;
        columns = 5;
    }

    g_battle_menu_thread_menu_data[3].inner_width = window_x;
    g_battle_menu_thread_menu_data[3].window_width = window_x;
    g_battle_menu_thread_menu_data[3].overall_width = window_x;
    g_battle_menu_thread_menu_data[3].header_id = row_mode;
    g_battle_menu_thread_menu_data[3].select_text_table = columns;
    for (i = 0; i < count; i++) {
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids[i] |= row_flags;
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            if (unit->mp - ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs[i] < 0
                && ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->enabled[i] != 0) {
                g_battle_menu_ability_display_flags[i] = 8;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT || type == ACTION_MENU_TYPE_MONSTER) {
            if ((((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->flags[i] & 2)
                && ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->enabled[i] != 0
                && (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SILENCE))) {
                g_battle_menu_ability_display_flags[i] = 8;
            }
        }
    }
    if (count == 0) {
        count = 1;
        g_battle_menu_thread_menu_data[3].header_id = 0;
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->ids[0] = TEXT_ID_ABILITY_NAME_BASE;
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->values[0] = 0;
        ((battle_menu_ability_list_t*)g_battle_ai_workspace_ptr)->extras[0] = 0;
        g_battle_menu_ability_display_flags[0] = 4;
    }
    g_battle_menu_thread_menu_data[3].window_y = 0x30;
    if (count >= 7) {
        g_battle_ability_menu_layout.hidden_rows = count - 6;
        g_battle_ability_menu_layout.row_count = 6;
    } else {
        g_battle_ability_menu_layout.hidden_rows = 0;
        g_battle_ability_menu_layout.row_count = count;
        g_battle_menu_thread_menu_data[3].window_y += (6 - count) * 8;
    }
    /* The menu thread's redraw, cursor and refresh parameters. */
    g_battle_thread_contexts[g_battle_current_thread_id].function_parameter_2 = 1;
    g_battle_thread_contexts[g_battle_current_thread_id].function_parameter_3 = 0;
    g_battle_thread_contexts[g_battle_current_thread_id].function_parameter_4 = 1;
}
