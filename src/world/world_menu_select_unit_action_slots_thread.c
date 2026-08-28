#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* Provisional: 17-byte per-unit menu row at g_world_menu_unit_selection_rows (compare
 * WORLD_UNKNOWN_ROW_BYTES in world_menu_remap_value_for_stored_unit). */

/* Menu entry 4's value and the loop counter share $s0 in the target: the
 * index dies after the two table loads and the register is reused by the
 * `sll s0,v0,0x1` that forms the counter, so they are one variable here.
 * &g_world_menu_action_slot_row_actions is written as an in-loop expression rather than a pre-loop
 * pointer local so that loop.c hoists it into the preheader after the bltz,
 * which is where the target's `lui/addiu s8` pair sits. */

/*
 * Thread body: walks the stored unit's menu row slots from the top value
 * down to zero, presenting each through the inner subroutine and writing
 * the chosen entry back into the row.
 */
void world_menu_select_unit_action_slots_thread(void) {
    s32 i = g_world_menu_thread_menu_data[4].selected_index;
    s32 menu = g_battle_action_menu_row_types[i];
    s32 check_unit = g_battle_menu_skillset_disabled_flags[i];
    world_menu_record_t* row = &g_world_menu_unit_selection_rows[g_world_unit_view_battle_id];
    u8* slot;
    u8* pair;
    s32 j;
    s16 text;

    for (i = (g_world_menu_restore_pending == 0) ? 2 : 0; i >= 0; i--) {
        world_menu_check_action_slot_restrictions(menu, i, check_unit);
        if (menu == 3) {
            slot = &row->bytes[i + 11] + i;
            g_world_menu_thread_menu_data[3].selected_index = slot[0];
            g_world_ability_menu_layout.row_offset = slot[1];
        } else {
            pair = &row->bytes[g_world_menu_thread_menu_data[4].selected_index * 2];
            g_world_menu_thread_menu_data[3].selected_index = pair[3];
            g_world_ability_menu_layout.row_offset = pair[4];
            slot = pair + 3;
        }
        for (j = 0; j < 6; j++) {
            if (menu != 3 || i == 0) {
                (&g_world_menu_action_slot_row_actions)[j] = 0x1024;
            } else {
                (&g_world_menu_action_slot_row_actions)[j] = -1;
            }
        }
        if (g_battle_action_menu_row_types[g_world_menu_thread_menu_data[4].selected_index] == 12) {
            g_world_menu_thread_menu_data[3].selected_index = 0;
            world_menu_run_companion_overlay_9_10_thread();
        }
        g_world_thread_inner_subroutine_callback = (void (*)(void))world_menu_build_ability_list;
        world_thread_call_on_main_stack(i);
        world_script_pulse_tutorial_wait_value(0xFB);
        world_menu_scrolling_list_thread();
        slot[0] = g_world_menu_thread_menu_data[3].selected_index;
        slot[1] = g_world_ability_menu_layout.row_offset;
        if (menu != 3) {
            world_thread_exit_current();
        }
        if (g_world_menu_action_slot_selected_option == -1) {
            if (i == 2) {
                world_thread_exit_current();
            }
            i += 2;
        } else {
            /* The workspace cell is void*; this menu reads it as a u16 text-id list. */
            text = ((u16*)g_battle_ai_workspace_ptr)[g_world_menu_thread_menu_data[3].selected_index] - 0x7000;
            if (i == 2) {
                g_world_menu_preview_action.calculator_type = text;
            }
            if (i == 1) {
                g_world_menu_preview_action.calculator_multiplier = text;
            }
            if (i == 0) {
                g_world_menu_preview_action.ability_id = text;
            }
        }
    }
    world_thread_exit_current();
}
