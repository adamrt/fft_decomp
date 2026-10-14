#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Menu entry 4's value and the loop counter share $s0 in the target: the
 * index dies after the two table loads and the register is reused by the
 * `sll s0,v0,0x1` that forms the counter, so they are one variable here.
 * g_battle_menu_action_slot_row_actions is written as an in-loop expression rather than a pre-loop
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
        battle_menu_check_action_slot_restrictions(menu, i, check_unit);
        if (menu == ACTION_MENU_TYPE_ARITHMETICKS) {
            slot = (menu_list_selection_t*)((u8*)row + (i + offsetof(menu_unit_selection_record_t, lists[4])) + i);
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index = slot->selected_index;
            g_battle_menu_ability_scroll_offset = slot->scroll_offset;
        } else {
            pair = (u8*)row
                + g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index * sizeof(menu_list_selection_t);
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index
                = pair[offsetof(menu_unit_selection_record_t, lists[0].selected_index)];
            g_battle_menu_ability_scroll_offset = pair[offsetof(menu_unit_selection_record_t, lists[0].scroll_offset)];
            slot = (menu_list_selection_t*)(pair + offsetof(menu_unit_selection_record_t, lists));
        }
        for (j = 0; j < 6; j++) {
            if (menu != ACTION_MENU_TYPE_ARITHMETICKS || i == 0) {
                g_battle_menu_action_slot_row_actions[j] = 0x1024;
            } else {
                g_battle_menu_action_slot_row_actions[j] = -1;
            }
        }
        if (g_action_menu_row_types[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index]
            == ACTION_MENU_TYPE_CHANGE_EQUIPMENT) {
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index = 0;
            battle_menu_run_equip_thread();
        }
        g_thread_call_target = battle_menu_build_ability_list;
        thread_call_on_main_stack(i);
        script_pulse_tutorial_wait_value(0xFB);
        menu_run_scrolling_ability_list_thread();
        slot->selected_index = g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index;
        slot->scroll_offset = g_battle_menu_ability_scroll_offset;
        if (menu != ACTION_MENU_TYPE_ARITHMETICKS) {
            thread_exit_current();
        }
        if (g_battle_menu_action_slot_selected_option == -1) {
            if (i == 2) {
                thread_exit_current();
            }
            i += 2;
        } else {
            text = g_ai_workspace_ptr->ability_list
                       .ids[g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index]
                - 0x7000;
            if (i == 2) {
                g_battle_menu_selected_action.calculator_type = text;
            }
            if (i == 1) {
                g_battle_menu_selected_action.calculator_multiplier = text;
            }
            if (i == 0) {
                g_battle_menu_selected_action.ability_id = text;
            }
        }
    }
    thread_exit_current();
}
