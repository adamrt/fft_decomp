#include "fft/battle.h"
#include "psx/types.h"

/* Menu entry 4's value and the loop counter share $s0 in the target: the
 * index dies after the two table loads and the register is reused by the
 * `sll s0,v0,0x1` that forms the counter, so they are one variable here.
 * &g_battle_menu_action_slot_row_actions is written as an in-loop expression rather than a pre-loop
 * pointer local so that loop.c hoists it into the preheader after the bltz,
 * which is where the target's `lui/addiu s8` pair sits. */

/*
 * Battle twin of world_menu_select_unit_action_slots_thread. Thread body: walks the stored unit's
 * menu record slots from the top value down to zero, presenting each through
 * the inner subroutine and writing the chosen entry back into the record.
 */
void battle_menu_select_unit_action_slots_thread(void) {
    s32 i = g_battle_menu_thread_menu_data[4].selected_index;
    s32 menu = g_battle_action_menu_row_types[i];
    s32 check_unit = g_battle_menu_skillset_disabled_flags[i];
    battle_menu_record_t* row = &g_battle_menu_unit_selection_records[g_battle_active_turn_unit.battle_id];
    u8* slot;
    u8* pair;
    s32 j;
    s16 text;

    for (i = (g_battle_menu_restore_pending == 0) ? 2 : 0; i >= 0; i--) {
        battle_menu_check_action_slot_restrictions(menu, i, check_unit);
        if (menu == 3) {
            slot = &row->bytes[i + 11] + i;
            g_battle_menu_thread_menu_data[3].selected_index = slot[0];
            g_battle_menu_ability_scroll_offset = slot[1];
        } else {
            pair = &row->bytes[g_battle_menu_thread_menu_data[4].selected_index * 2];
            g_battle_menu_thread_menu_data[3].selected_index = pair[3];
            g_battle_menu_ability_scroll_offset = pair[4];
            slot = pair + 3;
        }
        for (j = 0; j < 6; j++) {
            if (menu != 3 || i == 0) {
                (&g_battle_menu_action_slot_row_actions)[j] = 0x1024;
            } else {
                (&g_battle_menu_action_slot_row_actions)[j] = -1;
            }
        }
        if (g_battle_action_menu_row_types[g_battle_menu_thread_menu_data[4].selected_index] == 12) {
            g_battle_menu_thread_menu_data[3].selected_index = 0;
            battle_menu_run_companion_overlay_9_10_thread();
        }
        g_battle_thread_call_target = battle_menu_build_ability_list;
        battle_thread_call_on_main_stack(i);
        battle_script_pulse_tutorial_wait_value(0xFB);
        battle_menu_run_scrolling_ability_list_thread();
        slot[0] = g_battle_menu_thread_menu_data[3].selected_index;
        slot[1] = g_battle_menu_ability_scroll_offset;
        if (menu != 3) {
            battle_thread_exit_current();
        }
        if (g_battle_menu_action_slot_selected_option == -1) {
            if (i == 2) {
                battle_thread_exit_current();
            }
            i += 2;
        } else {
            text = ((u16*)g_battle_ai_workspace_ptr)[g_battle_menu_thread_menu_data[3].selected_index] - 0x7000;
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
    battle_thread_exit_current();
}
