#include "fft/battle.h"
#include "fft/world.h"

/*
 * Per-slot restriction check for the unit action menu, called once for each
 * of the rows walked by world_menu_select_unit_action_slots_thread.
 *
 * Picks a warning message for the stored unit (evade check, then the unit's
 * status bits, then the per-menu default from g_world_menu_action_slot_default_messages), or the menu-3
 * "no entry" message 0x8009 - value. When a message applies it is written to
 * menu entry 35 and shown by world_menu_icon_linked_entry_thread; mode 2
 * additionally plays the invalid sound and exits the calling thread, so the
 * slot is rejected. With no message the slot is confirmed silently.
 *
 * `message = 0x1007` sits inside the status-bit arm rather than ahead of the
 * inner `if`. Both spellings are equivalent (the other arm overwrites it and
 * GCC drops the dead store either way), but hoisting it makes $17 look live
 * across the evade branch, and reorg then refuses to lift `message = 0x1003`
 * into the bne delay slot, swapping the two delay-slot words at +0x64/+0x6c.
 * Written this way, reorg fills the bne slot with `message` and the jump slot
 * with `mode`, which is the target's order.
 */

void world_menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit) {
    s32 mode;
    s32 message;
    void* parameter;
    battle_stats_t* unit;

    g_world_active_menu = menu;
    parameter = world_thread_get_current_parameter_1();
    mode = 0;
    message = 0;
    unit = world_unit_get_battle_stats_for_stored();
    if (check_unit != 0) {
        if (battle_formula_can_unit_evade(unit) == 2) {
            message = 0x1003;
            mode = 2;
        } else {
            if (unit->status_sets.current[2] & 2) {
                message = 0x1007;
                mode = 2;
            } else {
                mode = 2;
                message = g_world_menu_action_slot_default_messages[menu] + 0x1000;
            }
        }
    }
    if (message == 0 && menu == 3) {
        message = 0x8009 - value;
        mode = 1;
    }
    if (mode != 0) {
        if ((g_main_game_options.value & GAME_OPTIONS_NAVIGATION_MESSAGES_MASK)
                != GAME_OPTIONS_NAVIGATION_MESSAGES_OFF_BITS
            || (message & 0xff00) != (g_main_game_options.value & GAME_OPTIONS_NAVIGATION_MESSAGES_MASK)) {
            g_world_menu_thread_menu_data[35].text_id = message;
            world_thread_start(g_world_thread_current_id - 1, world_menu_icon_linked_entry_thread);
            world_thread_set_parameters(g_world_thread_current_id - 1, (s32)&g_world_menu_thread_menu_data[35], 0, 0);
        }
        if (mode == 2) {
            world_sound_set_effect_to_invalid();
            if ((g_main_game_options.value & GAME_OPTIONS_NAVIGATION_MESSAGES_MASK) == 0) {
                world_thread_wait_until_inactive(g_world_thread_current_id - 1);
            }
            world_thread_exit_current();
        }
    } else {
        mode = g_world_menu_row_type_confirm_actions
            [g_battle_action_menu_row_types[g_world_menu_thread_menu_data[4].selected_index]][0];
        if (mode < 0) {
            g_world_menu_action_slot_row_actions = mode;
            world_menu_confirm_action_silently((s32)parameter);
            world_thread_exit_current();
        }
    }
    g_world_menu_restore_depth = 2;
}
