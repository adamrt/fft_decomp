#include "fft/main_unit.h"
#include "fft/option.h"
#include "fft/text.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Installs the dead-unit result panel as the active menu descriptor, rebuilds
 * its selectable list from the context bitmap, then runs the panel's worker
 * threads until the player commits an action (0x801c0e80-0x801c14ec).
 *
 * Source-order constraints proven by the byte-exact build:
 *   - `g_battle_menu_thread_menu_data` must be assigned from `&g_dead_unit_menu` *before*
 *     `menu`, so the shared address stays a copy (`move`) instead of being
 *     folded into a single definition.
 *   - `g_dead_unit_menu.menu_type` must be stored *after* both of those, or
 *     CSE bases the plain object address on `symbol + 0x3a` and derives
 *     `&g_dead_unit_menu` with an extra `addiu`.
 *   - `battle_thread_exit_current()` does not return, and the original relies
 *     on that: no `return` follows the early exits, so their epilogues fall
 *     through into the next case instead of being cross-jumped to the shared
 *     tail. */
void option_menu_start_dead_unit_threads(void) {
    dead_unit_context_t* context = g_dead_unit_context;
    world_menu_entry_t* previous_menu_descriptor = g_battle_menu_thread_menu_data;
    dead_unit_menu_t* menu;
    s32 value_count;
    s32 byte_index;
    s32 bit_index;
    s32 index;
    s16 message_id;

    g_battle_menu_thread_menu_data = (world_menu_entry_t*)&g_dead_unit_menu;
    menu = &g_dead_unit_menu;
    g_dead_unit_menu.menu_type = 5;
    g_dead_unit_menu.entries = &g_dead_unit_menu_entries;

    for (index = 0x1ff; index >= 0; index--) {
        g_dead_unit_menu_flags[index] = 0;
    }

    value_count = 0;
    for (byte_index = 0; byte_index < 0x40; byte_index++) {
        s32 mask = 1;
        /* A pointer local, not `context->flags[byte_index]` in the test: the
         * indexed form folds 0x53 into the load displacement, while the
         * target biases the index and loads at 0(reg). */
        u8* flag_byte = &context->flags[byte_index];

        for (bit_index = 0; bit_index < 8; bit_index++) {
            if ((*flag_byte & mask) != 0) {
                /* Separate statement: written as one expression, fold
                 * reassociates the ability-name base with `byte_index * 8` and loop.c turns
                 * the pair into a strength-reduced giv. */
                s32 value = bit_index + TEXT_ID_ABILITY_NAME_BASE;

                g_dead_unit_menu_values[value_count++] = byte_index * 8 + value;
            }
            mask *= 2;
        }
    }

    menu->window_x = 0xc8;
    menu->window_y = 0x40;
    menu->window_width = 0x70;
    menu->overall_width = 0x70;
    menu->inner_width = 0x70;
    if (value_count < 8) {
        menu->entries->visible_count = value_count;
        menu->entries->overflow_count = 0;
    } else {
        menu->entries->visible_count = 8;
        menu->entries->overflow_count = value_count - 8;
    }

    if (context->kind == 0) {
        message_id = 0x183c;
        ((dead_unit_menu_t*)g_battle_menu_thread_menu_data)->message_id = message_id;
        battle_thread_start(g_battle_current_thread_id - 1, battle_menu_icon_linked_entry_thread);
        battle_thread_set_parameters(g_battle_current_thread_id - 1,
            (s32)((dead_unit_menu_t*)g_battle_menu_thread_menu_data)->message_menu, 0, 0);
        battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
        battle_thread_wait_frames(2);
        g_dead_unit_action = 0;
        g_battle_menu_thread_menu_data = previous_menu_descriptor;
        g_battle_menu_pending_selection[0] = 0;
        battle_thread_exit_current();
    }

    g_dead_unit_result = 0;
    if (context->kind == 4) {
        if (main_item_get_total_equipment_quantity(context->item_id, 1) == 0x63) {
            g_dead_unit_roster_id = context->item_id;
            g_dead_unit_roster_id_copy = context->item_id;
            message_id = 0x101b;
            g_dead_unit_item_value = main_item_get_data_pointer(context->item_id)->price >> 2;
            g_dead_unit_result = context->item_id;
        } else {
            message_id = 0x101a;
            g_dead_unit_roster_id = context->item_id;
            g_dead_unit_result = context->item_id;
        }
        ((dead_unit_menu_t*)g_battle_menu_thread_menu_data)->message_id = message_id;
        battle_thread_start(g_battle_current_thread_id - 1, battle_menu_icon_linked_entry_thread);
        battle_thread_set_parameters(g_battle_current_thread_id - 1,
            (s32)((dead_unit_menu_t*)g_battle_menu_thread_menu_data)->message_menu, 0, 0);
        battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
        battle_thread_wait_frames(2);
        g_dead_unit_action = 4;
        g_battle_menu_thread_menu_data = previous_menu_descriptor;
        g_battle_menu_pending_selection[0] = 0;
        battle_thread_exit_current();
    }

    battle_thread_set_parameters(10, 0, 0, 1);
    battle_thread_set_parameters(11, 0, 0, 1);
    battle_thread_set_parameters(12, 0, 0, 1);
    battle_thread_set_parameters(13, 0, 0, 1);
    battle_thread_wait_until_inactive(10);
    battle_thread_wait_until_inactive(11);
    battle_thread_wait_until_inactive(12);
    battle_thread_wait_until_inactive(13);

    for (;;) {
        if (context->kind == 3) {
            g_dead_unit_secondary_result = 0xff;
            battle_thread_start(g_battle_current_thread_id - 1, battle_menu_run_icon_selection_loop);
            battle_thread_set_parameters(g_battle_current_thread_id - 1,
                (s32)((dead_unit_menu_t*)g_battle_menu_thread_menu_data)->move_menu, 0, 0);
            battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
        }

        g_dead_unit_primary_result = 0xff;
        if ((context->kind == 3 && g_dead_unit_secondary_result == 0) || context->kind == 1) {
            battle_thread_start(g_battle_current_thread_id - 2, option_build_at_list);
            battle_thread_set_parameters(g_battle_current_thread_id - 2, (s32)g_battle_menu_thread_menu_data, 0, 0);
            battle_thread_start(10, battle_menu_icon_linked_entry_thread);
            battle_thread_set_parameters(
                10, (s32)((dead_unit_menu_t*)g_battle_menu_thread_menu_data)->at_list_menu, 0, 0);
            for (;;) {
                battle_thread_yield();
                if (battle_thread_is_running_8014cc94(g_battle_current_thread_id - 3) != 0) {
                    battle_thread_start(9, option_thread_wait_forever);
                } else {
                    battle_thread_suspend(9);
                }
                if (battle_thread_is_running_8014cc94(g_battle_current_thread_id - 1) == 0) {
                    break;
                }
                g_dead_unit_controller_input = battle_script_get_controller_input_pointer(0);
                if ((*g_dead_unit_controller_input & PSX_PAD_SELECT) != 0) {
                    battle_menu_start_building_thread();
                }
            }

            battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
            battle_thread_set_parameters(10, 0, 0, 1);
            battle_thread_suspend(9);
            if (g_dead_unit_primary_result == 0) {
                g_dead_unit_action = 1;
                break;
            }
            if (g_dead_unit_primary_result == 1 || (g_dead_unit_primary_result & 0xff) == 0xff) {
                if (context->kind == 3) {
                    continue;
                }
            }
        }

        if ((g_dead_unit_secondary_result == 1 && context->kind == 3) || context->kind == 2) {
            g_dead_unit_primary_result = 0xff;
            battle_thread_start(g_battle_current_thread_id - 1, battle_menu_icon_linked_entry_thread);
            battle_thread_set_parameters(g_battle_current_thread_id - 1,
                (s32)((dead_unit_menu_t*)g_battle_menu_thread_menu_data)->confirm_menu, 0, 0);
            battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
            if (g_dead_unit_primary_result == 0) {
                g_dead_unit_action = 2;
                break;
            }
            if (context->kind != 2) {
                continue;
            }
        }
        g_dead_unit_action = 0;
        break;
    }

    battle_thread_wait_frames(0x14);
    g_battle_menu_thread_menu_data = previous_menu_descriptor;
    g_battle_menu_pending_selection[0] = 0;
    battle_thread_exit_current();
}
