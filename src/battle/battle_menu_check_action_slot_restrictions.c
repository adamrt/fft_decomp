#include "fft/battle.h"
#include "psx/types.h"

/*
 * Per-slot restriction check for the unit action menu, the BATTLE twin of
 * world_menu_check_action_slot_restrictions (0x800f454c); the two are masked-identical.
 *
 * Picks a warning message for the attacking unit (evade check, then the unit's
 * status bits, then the per-menu default from g_battle_menu_restriction_message_ids), or the menu-3
 * "no entry" message 0x8009 - value. When a message applies it is written to
 * menu entry 35 and shown by battle_menu_icon_linked_entry_thread; mode 2
 * additionally plays the invalid sound and exits the calling thread, so the
 * slot is rejected. With no message the slot is confirmed silently.
 * WORLD twin: world_menu_check_action_slot_restrictions.
 *
 * `message = 0x1007` sits inside the status-bit arm rather than ahead of the
 * inner `if`. Both spellings are equivalent (the other arm overwrites it and
 * GCC drops the dead store either way), but hoisting it makes $17 look live
 * across the evade branch, and reorg then refuses to lift `message = 0x1003`
 * into the bne delay slot, swapping the two delay-slot words at +0x64/+0x6c.
 * Written this way, reorg fills the bne slot with `message` and the jump slot
 * with `mode`, which is the target's order.
 */

void battle_menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit) {
    s32 mode;
    s32 message;
    s32 parameter;
    battle_stats_t* unit;

    g_battle_active_menu = menu;
    parameter = battle_thread_get_current_parameter_1();
    mode = 0;
    message = 0;
    unit = battle_unit_get_attacker_data_pointer();
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
                message = g_battle_menu_restriction_message_ids[menu] + 0x1000;
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
            g_battle_menu_thread_menu_data[35].text_id = message;
            battle_thread_start(g_battle_current_thread_id - 1, battle_menu_icon_linked_entry_thread);
            battle_thread_set_parameters(
                g_battle_current_thread_id - 1, (s32)&g_battle_menu_thread_menu_data[35], 0, 0);
        }
        if (mode == 2) {
            battle_sound_set_effect_to_invalid();
            if ((g_main_game_options.value & GAME_OPTIONS_NAVIGATION_MESSAGES_MASK) == 0) {
                battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
            }
            battle_thread_exit_current();
        }
    } else {
        mode = g_battle_menu_action_type_modes
            [g_battle_action_menu_row_types[g_battle_menu_thread_menu_data[4].selected_index]][0];
        if (mode < 0) {
            g_battle_menu_action_slot_row_actions = mode;
            battle_menu_confirm_action_silently((void*)parameter);
            battle_thread_exit_current();
        }
    }
    g_battle_menu_restore_depth = 2;
}
