#include "fft/battle.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Act on a confirmed menu option when circle is pressed.
 *
 * An action in 0..0xff starts that idle-action menu entry's thread on the
 * thread below the current one (bit 0x1000 suppresses the confirm sound);
 * 0x4d2 records the option as -1; a negative action -n closes n menu levels
 * by setting the third parameter of the next n threads (thread 8 also saves
 * the open menus' selections). */
void menu_handle_action(menu_entry_t* menu, s32 selected_option) {
    s16* actions;
    s32 action;
    s32 silent;
    s32 i;
    s32 j;
    s32 thread_id;

    if (g_menu_input_mode == MENU_INPUT_TUTORIAL
        && ((script_get_variable(EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS) >> selected_option) & 1)) {
        return;
    }
    silent = 0;
    if (!(g_script_event_input & PSX_PAD_CIRCLE) || menu_can_accept_input() == 0) {
        return;
    }
    if (g_thread_contexts[g_thread_current_id].task_words[1] == 0) {
        g_thread_contexts[g_thread_current_id].task_words[1] = 2;
        g_thread_contexts[g_thread_current_id].task_words[0] = 0;
    }
    actions = menu->row_actions;
    if ((s32)actions >= -15 && (s32)actions <= -1) {
        action = (s32)actions;
    } else {
        action = actions[selected_option];
    }
    if (action >= 0) {
        if (action & MENU_ROW_ACTION_SILENT) {
            silent = 1;
            /* Zero-instruction use: raises silent's allocation priority so it
               takes $s6 and selected_option keeps $s7, as in the target. */
            __asm__("" : : "r"(silent), "r"(silent));
            action -= MENU_ROW_ACTION_SILENT;
        }
    }
    if (action >= 0) {
        if (action < 0x100) {
            thread_start(g_thread_current_id - 1, g_menu_thread_menu_data[action].thread_entry);
            thread_set_parameters(g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[action], 0, 0);
        } else if (action == MENU_ROW_ACTION_CLEAR_SELECTION) {
            selected_option = -1;
        }
    } else {
        action = -action;
        for (i = 0; i < THREAD_SLOT_COUNT; i++) {
            if (g_thread_contexts[i].task_id == THREAD_TASK_RESUME) {
                g_thread_contexts[i].task_id = THREAD_TASK_STOP_REQUEST;
            }
        }
        for (i = 0; i < action; i++) {
            thread_id = i + g_thread_current_id;
            g_thread_contexts[thread_id].function_parameter_3 = 1;
            if (thread_id == 8) {
                for (j = 0; j < action; j++) {
                    if (thread_is_running_asm_variant(8 - j) != 0) {
                        g_menu_pending_selection[j]
                            = ((menu_entry_t*)g_thread_contexts[8 - j].function_parameter_1)->selected_index;
                    }
                }
            }
        }
    }
    g_script_event_input = 0;
    *menu_entry_result(menu) = selected_option;
    if (g_sound_suppressed == 0 && silent == 0) {
        sound_queue_sfx_confirm();
    }
}
