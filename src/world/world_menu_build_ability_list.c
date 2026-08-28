/* Builds the ability list for the active skillset menu entry (menu entry 4's
 * selected_index selects the skillset; g_battle_action_menu_row_types gives its list type). Each type
 * fills the 0x52-row work buffer at *g_battle_ai_workspace_ptr through its own loader, then
 * the rows are sized, greyed (not enough MP, or silenced for voice abilities)
 * and the window layout at 0x80153c78 is set up for 6 visible rows.
 * WORLD twin of battle_menu_build_ability_list (0x8013fa6c); the two are masked-identical.
 *
 * Matching notes: the layout block is one struct (its values_x/extras_x are s16, so `-= 4`
 * stays an addiu immediate and the constant is not shared); window_x is a
 * separate local from the unit index, and its updates follow the loader
 * calls in each arm. The ff-terminated copy takes an ignored third argument
 * (0 or the row count), as the target's call sites load $a2. */
#include "fft/battle.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/text.h"
#include "fft/world.h"
#include "psx/types.h"

extern void* g_battle_ai_workspace_ptr; /* pointer cell reloaded per subsystem */
extern world_menu_entry_t* g_world_menu_thread_menu_data;
extern s16 g_world_unit_view_battle_id; /* selected unit index */
extern u8 g_battle_action_menu_row_types[];
extern s16 g_world_menu_row_type_confirm_actions[][2]; /* per row type: [1] == 1 lists by id */
extern u8 g_main_item_quantities[];
extern battle_ai_command_action_t g_world_menu_preview_action;
extern battle_stats_t* world_unit_get_battle_stats_for_stored(void);
extern s32 world_menu_widen_bytes_to_halfwords(u16* destination, const u8* source);
extern s32 battle_menu_display_item_inventory_ability(s32 unit, s32 skillset, u8* items);
extern s32 battle_menu_load_throw_abilities(s32 unit, s32 skillset, u8* items);
extern s32 battle_menu_load_math_skill_attributes(s32 unit, s32 skillset, s16* ids);
extern s32 battle_menu_load_math_skill_multiples(s32 unit, s32 skillset, s16* ids);
extern s32 battle_menu_collect_calculator_abilities(s32 unit, u8 skillset, s16* ids);
extern s32 battle_menu_collect_monster_skill_abilities(s32 unit, u8 skillset, s16* ids, s32 zero, u8* flags);
extern s32 battle_menu_load_draw_out_abilities(s32 unit, s32 skillset, u8* items);
extern s32 battle_menu_load_charge_skillset(s32 unit_id, s32 skillset, s16* ability_ids, u8* ct_out, u8* turns_out);

void world_menu_build_ability_list(s32 mode) {
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

    g_world_ability_menu_layout.ids = ((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids;
    g_world_ability_menu_layout.values = ((world_ability_list_t*)g_battle_ai_workspace_ptr)->values;
    g_world_ability_menu_layout.extras = ((world_ability_list_t*)g_battle_ai_workspace_ptr)->extras;
    unit_index = g_world_unit_view_battle_id;
    unit = world_unit_get_battle_stats_for_stored();
    type = g_battle_action_menu_row_types[g_world_menu_thread_menu_data[4].selected_index];
    skillset = g_world_action_menu_skillsets[g_world_menu_thread_menu_data[4].selected_index];
    g_world_menu_preview_action.skillset = skillset;
    for (i = 0; i < 0x50; i++) {
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids[i] = 0xFFFF;
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs[i] = 0xFF;
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_23e[i] = 1;
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->values[i] = 0;
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->extras[i] = 0xFFFF;
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->flags[i] = 0;
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2[i] = 1;
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->enabled[i] = 0xFF;
        g_world_menu_ability_display_flags[i] = 0;
    }
    ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs[0x50] = 0xFF;
    ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_23e[0x50] = 0xFF;
    ((world_ability_list_t*)g_battle_ai_workspace_ptr)->flags[0x50] = 0xFF;
    ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2[0x50] = 0xFF;
    ((world_ability_list_t*)g_battle_ai_workspace_ptr)->enabled[0x50] = 0xFF;

    if (type == ACTION_MENU_TYPE_DEFAULT) {
        battle_menu_get_unit_skillset_ability_data(unit_index, skillset,
            (s16*)((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_23e, 0,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->flags,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2);
        world_script_copy_bytes(((world_ability_list_t*)g_battle_ai_workspace_ptr)->enabled,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_23e, 0x50);
    }
    if (type == ACTION_MENU_TYPE_ITEM_INVENTORY) {
        /* The target also passes mp_costs in a2 to this two-argument loader. */
        ((void (*)(s32, s32, u8*))battle_menu_display_item_inventory_ability)(
            unit_index, skillset, ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs);
    }
    if (type == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        /* The target also passes mp_costs in a2 to this two-argument loader. */
        ((void (*)(s32, s32, u8*))battle_menu_load_throw_abilities)(
            unit_index, skillset, ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs);
    }
    if (type == ACTION_MENU_TYPE_ARITHMETICKS) {
        if (mode == 2) {
            /* The definition types the id buffer parameter as s32. */
            ((void (*)(s32, s32, u16*))battle_menu_load_math_skill_attributes)(
                unit_index, skillset, ((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids);
        }
        if (mode == 1) {
            /* The definition types the id buffer parameter as s32. */
            ((void (*)(s32, s32, u16*))battle_menu_load_math_skill_multiples)(
                unit_index, skillset, ((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids);
        }
        if (mode == 0) {
            battle_menu_collect_calculator_abilities(
                unit_index, skillset, (s16*)((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids);
        }
    }
    if (type == ACTION_MENU_TYPE_MONSTER) {
        battle_menu_collect_monster_skill_abilities(unit_index, skillset,
            (s16*)((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids, 0,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->flags);
    }
    if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        /* The target also passes mp_costs in a2 to this two-argument loader. */
        ((void (*)(s32, s32, u8*))battle_menu_load_draw_out_abilities)(
            unit_index, skillset, ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs);
    }
    if (type == ACTION_MENU_TYPE_CHARGE) {
        /* The definition types the three output buffer parameters as s32. */
        ((void (*)(s32, s32, u16*, u8*, u8*))battle_menu_load_charge_skillset)(unit_index, skillset,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2);
        world_script_copy_bytes(((world_ability_list_t*)g_battle_ai_workspace_ptr)->enabled,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs, 0x50);
    }
    window_x = 0xAC;
    if (type != ACTION_MENU_TYPE_ARITHMETICKS) {
        mode = 0;
    }

    g_world_ability_menu_layout.values_mode = 3;
    g_world_ability_menu_layout.extras_mode = 3;
    g_world_ability_menu_layout.values_x = 0x6A;
    g_world_ability_menu_layout.extras_x = 0x7E;
    g_world_ability_menu_layout.ids_mode = 0;
    if (g_world_menu_row_type_confirm_actions[type][1] == 1) {
        /* The target also passes 0 in a2 to this two-argument copy. */
        count = ((s32 (*)(void*, void*, s32))world_menu_widen_bytes_to_halfwords)(
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs, 0);
        world_copy_bytes_to_s16_array((s16*)((world_ability_list_t*)g_battle_ai_workspace_ptr)->extras,
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2, count);
        found = 0;
        for (i = 0; i < count; i++) {
            ((world_ability_list_t*)g_battle_ai_workspace_ptr)->values[i]
                = g_main_item_quantities[((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids[i]];
            if (((world_ability_list_t*)g_battle_ai_workspace_ptr)->values[i] == 0) {
                g_world_menu_ability_display_flags[i] = 4;
            } else {
                found = 1;
            }
        }
        row_mode = 7;
        if (found != 0) {
            window_x -= 4;
        } else {
            g_world_ability_menu_layout.extras_mode = 2;
            world_copy_bytes_to_s16_array((s16*)((world_ability_list_t*)g_battle_ai_workspace_ptr)->values,
                ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2, count);
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
            if (((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids[count] == 0xFFFF) {
                break;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            for (i = 0; i < count; i++) {
                if (((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs[i] != 0) {
                    found = 1;
                    break;
                }
            }
        }
        if (type == ACTION_MENU_TYPE_ARITHMETICKS && mode > 0) {
            window_x = 0x88;
            g_world_ability_menu_layout.values_mode = 2;
            g_world_ability_menu_layout.extras_mode = 2;
            row_mode = 9;
        } else {
            if (found != 0 && type == ACTION_MENU_TYPE_DEFAULT) {
                /* The target also passes the row count in a2 to this two-argument copy. */
                ((s32 (*)(void*, void*, s32))world_menu_widen_bytes_to_halfwords)(
                    ((world_ability_list_t*)g_battle_ai_workspace_ptr)->values,
                    ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs, count);
                window_x -= 8;
                world_copy_bytes_to_s16_array((s16*)((world_ability_list_t*)g_battle_ai_workspace_ptr)->extras,
                    ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2, count);
                row_mode = 6;
                g_world_ability_menu_layout.values_x -= 8;
                g_world_ability_menu_layout.extras_x -= 8;
            } else {
                world_copy_bytes_to_s16_array((s16*)((world_ability_list_t*)g_battle_ai_workspace_ptr)->values,
                    ((world_ability_list_t*)g_battle_ai_workspace_ptr)->bytes_2e2, count);
                window_x = 0x94;
                row_mode = 5;
                g_world_ability_menu_layout.extras_mode = 2;
                g_world_ability_menu_layout.values_x -= 4;
                g_world_ability_menu_layout.extras_x -= 4;
            }
        }
        row_flags = TEXT_ID_ABILITY_NAME_BASE;
        columns = 5;
    }

    g_world_menu_thread_menu_data[3].inner_width = window_x;
    g_world_menu_thread_menu_data[3].window_width = window_x;
    g_world_menu_thread_menu_data[3].overall_width = window_x;
    g_world_menu_thread_menu_data[3].header_id = row_mode;
    g_world_menu_thread_menu_data[3].select_text_table = columns;
    for (i = 0; i < count; i++) {
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids[i] |= row_flags;
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            if (unit->mp - ((world_ability_list_t*)g_battle_ai_workspace_ptr)->mp_costs[i] < 0
                && ((world_ability_list_t*)g_battle_ai_workspace_ptr)->enabled[i] != 0) {
                g_world_menu_ability_display_flags[i] = 8;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT || type == ACTION_MENU_TYPE_MONSTER) {
            if ((((world_ability_list_t*)g_battle_ai_workspace_ptr)->flags[i] & 2)
                && ((world_ability_list_t*)g_battle_ai_workspace_ptr)->enabled[i] != 0
                && (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SILENCE))) {
                g_world_menu_ability_display_flags[i] = 8;
            }
        }
    }
    if (count == 0) {
        count = 1;
        g_world_menu_thread_menu_data[3].header_id = 0;
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->ids[0] = TEXT_ID_ABILITY_NAME_BASE;
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->values[0] = 0;
        ((world_ability_list_t*)g_battle_ai_workspace_ptr)->extras[0] = 0;
        g_world_menu_ability_display_flags[0] = 4;
    }
    g_world_menu_thread_menu_data[3].window_y = 0x30;
    if (count >= 7) {
        g_world_ability_menu_layout.hidden_rows = count - 6;
        g_world_ability_menu_layout.visible_rows = 6;
    } else {
        g_world_ability_menu_layout.hidden_rows = 0;
        g_world_ability_menu_layout.visible_rows = count;
        g_world_menu_thread_menu_data[3].window_y += (6 - count) * 8;
    }
    /* Menu-thread parameters 2-4 request a redraw, reset the cursor and
     * request a refresh. */
    g_world_thread_contexts[g_world_thread_current_id].function_parameter_2 = 1;
    g_world_thread_contexts[g_world_thread_current_id].function_parameter_3 = 0;
    g_world_thread_contexts[g_world_thread_current_id].function_parameter_4 = 1;
}
