#include "fft/battle.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Provisional view of the menu entry passed to the action handler: the
 * action word at 0x24 is either a small negative code (-15..-1) or a pointer
 * to one s16 action per option; 0x34 points at the halfword that receives
 * the chosen option. Same stride as battle_menu_idle_action_entry_t. */
typedef struct battle_menu_action_entry {
    u8 unknown_00[0x24];
    s16* actions; /* 0x24 */
    u8 unknown_28[0xC];
    s16* selection; /* 0x34 */
    u8 unknown_38[4];
} battle_menu_action_entry_t;

/* Act on a confirmed menu option when circle is pressed.
 *
 * An action in 0..0xff starts that idle-action menu entry's thread on the
 * thread below the current one (bit 0x1000 suppresses the confirm sound);
 * 0x4d2 records the option as -1; a negative action -n closes n menu levels
 * by setting the third parameter of the next n threads (thread 8 also saves
 * the open menus' selections). */
void battle_menu_handle_action(void* menu, s32 selected_option) {
    battle_menu_action_entry_t* entry;
    s16* actions;
    s32 action;
    s32 silent;
    s32 i;
    s32 j;
    s32 thread_id;

    entry = menu;
    if (g_battle_menu_input_disabled == 2
        && ((battle_script_get_variable(EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS) >> selected_option) & 1)) {
        return;
    }
    silent = 0;
    if (!(g_battle_script_event_input & PSX_PAD_CIRCLE) || battle_menu_can_accept_input() == 0) {
        return;
    }
    if (g_battle_thread_contexts[g_battle_current_thread_id].task_words[1] == 0) {
        g_battle_thread_contexts[g_battle_current_thread_id].task_words[1] = 2;
        g_battle_thread_contexts[g_battle_current_thread_id].task_words[0] = 0;
    }
    actions = entry->actions;
    if ((u32)((s32)actions + 15) < 15) {
        action = (s32)actions;
    } else {
        action = actions[selected_option];
    }
    if (action >= 0) {
        if (action & 0x1000) {
            silent = 1;
            /* Zero-instruction use: raises silent's allocation priority so it
               takes $s6 and selected_option keeps $s7, as in the target. */
            __asm__("" : : "r"(silent), "r"(silent));
            action -= 0x1000;
        }
    }
    if (action >= 0) {
        if (action < 0x100) {
            battle_thread_start(g_battle_current_thread_id - 1, g_battle_menu_thread_menu_data[action].thread_entry);
            battle_thread_set_parameters(
                g_battle_current_thread_id - 1, (s32)&g_battle_menu_thread_menu_data[action], 0, 0);
        } else if (action == 0x4D2) {
            selected_option = -1;
        }
    } else {
        action = -action;
        for (i = 0; i < 16; i++) {
            if (g_battle_thread_contexts[i].task_id == NATIVE_THREAD_TASK_RESUME) {
                g_battle_thread_contexts[i].task_id = NATIVE_THREAD_TASK_STOP_REQUEST;
            }
        }
        for (i = 0; i < action; i++) {
            thread_id = i + g_battle_current_thread_id;
            g_battle_thread_contexts[thread_id].function_parameter_3 = 1;
            if (thread_id == 8) {
                for (j = 0; j < action; j++) {
                    if (battle_thread_is_running_8014cc94(8 - j) != 0) {
                        g_battle_menu_pending_selection[j]
                            = ((battle_menu_idle_action_entry_t*)g_battle_thread_contexts[8 - j].function_parameter_1)
                                  ->selected_index;
                    }
                }
            }
        }
    }
    g_battle_script_event_input = 0;
    *entry->selection = selected_option;
    if (g_battle_sound_suppressed == 0 && silent == 0) {
        battle_sound_set_effect_to_confirm();
    }
}
