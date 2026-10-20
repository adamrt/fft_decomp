/* Menu-row actions, cancellation and input gating for the event menus.
 * Restoring a remembered choice uses the same row-action flags as player input. */

#include "fft/command.h"
#include "fft/help.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_input.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Returns 1 when a menu thread may act on input: no previous event thread
 * running, no third thread parameter (a stop request), not in event mode, no
 * help menu open, and g_menu_input_block_frames clear. In WORLD, a help box
 * (box type 0x30) in the text or message slot blocks input first and arms a
 * five-frame block. */
s32 menu_can_accept_input(void) {
#ifdef FFT_WORLD
    if ((thread_is_running_asm(THREAD_SCREEN_SLOT_TEXT)
            && (g_thread_contexts[THREAD_SCREEN_SLOT_TEXT].function_parameter_1 & 0x70) == 0x30)
        || (thread_is_running_asm(THREAD_SCREEN_SLOT_MESSAGE)
            && (g_thread_contexts[THREAD_SCREEN_SLOT_MESSAGE].function_parameter_1 & 0x70) == 0x30)) {
        g_menu_input_block_frames = 5;
        return 0;
    }
#endif
    if (thread_is_previous_running() != 0 || thread_current_parameter_3_get() != 0 || g_menu_input_lock_mode != 0
        || g_help_open != 0 || g_menu_input_block_frames != 0) {
        return 0;
    }
    return 1;
}

/* Act on Circle for menu row row_index: start the row's menu thread one slot below the current
 * one, clear the selection, or for a negative action -n ask n menu threads from the current one up
 * to close. Stores the row as the entry's result and plays the confirm sound unless the action is
 * silent; in tutorial input mode rows disabled by EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS do
 * nothing. */
void menu_handle_action(menu_entry_t* entry, s32 row_index) {
    s32 silent;
    s32 target;
    s32 i;
    s32 j;
    s32 thread_id;
    s32 menu_thread_id;
    s16* row_actions;
    s16* result;

    if (g_menu_input_mode == MENU_INPUT_TUTORIAL
        && ((script_variable_get(EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS) >> row_index) & 1)) {
        return;
    }
    silent = 0;
    if ((g_script_event_input & PSX_PAD_CIRCLE) && menu_can_accept_input() != 0) {
        if (g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] == 0) {
            g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 2;
            g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
        }
        row_actions = entry->row_actions;
        target = ((s32)row_actions >= -0xf && (s32)row_actions <= -1) ? (s32)row_actions : row_actions[row_index];
        if (target >= 0) {
            if (target & MENU_ROW_ACTION_SILENT) {
                silent = 1;
                /* Raises silent's allocation priority; otherwise $s6-$s8 rotate. */
                __asm__("" : : "r"(silent), "r"(silent));
                target -= MENU_ROW_ACTION_SILENT;
            }
        }
        if (target >= 0) {
            if (target < 0x100) {
                thread_start(g_thread_current_id - 1, g_menu_thread_menu_data[target].thread_entry);
                thread_parameters_set(g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[target], 0, 0);
            } else if (target == MENU_ROW_ACTION_CLEAR_SELECTION) {
                row_index = -1;
            }
        } else {
            target = -target;
            for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
                if (g_thread_contexts[i].task_id == THREAD_TASK_RESUME) {
                    g_thread_contexts[i].task_id = THREAD_TASK_STOP_REQUEST;
                }
            }
            /* Remember the rows of the closing battle menus (the menu slot and its
             * children below it) as the pending selection. */
            for (i = 0; i < target; i++) {
                thread_id = i + g_thread_current_id;
                g_thread_contexts[thread_id].function_parameter_3 = 1;
                if (thread_id == THREAD_SLOT_MENU) {
                    for (j = 0; j < target; j++) {
                        menu_thread_id = THREAD_SLOT_MENU - j;
                        if (thread_is_running_asm(menu_thread_id) != 0) {
                            g_command_pending_selection[j]
                                = ((menu_entry_t*)g_thread_contexts[menu_thread_id].function_parameter_1)
                                      ->selected_index;
                        }
                    }
                }
            }
        }
        g_script_event_input = 0;
        result = entry->value;
        *result = row_index;
        if (g_sound_suppressed == 0 && silent == 0) {
            sound_confirm_queue();
        }
    }
}

/* While the menu may accept input, move a row cursor with Up and Down, wrapping
 * between row 0 and the entry's max_row_index, and play the cursor sound. */
void menu_row_cursor_update(menu_entry_t* ctx, s32* sel) {
    s32 selection;
    s32 max;
    s32 cur;

    if (menu_can_accept_input() != 0) {
        if (g_script_event_input & PSX_PAD_UP) {
            selection = *sel;
            if (selection == 0) {
                selection = ctx->max_row_index;
            } else {
                selection = selection - 1;
            }
            *sel = selection;
            sound_cursor_queue();
        }
        if (g_script_event_input & PSX_PAD_DOWN) {
            max = ctx->max_row_index;
            cur = *sel;
            if (cur == max) {
                *sel = 0;
            } else {
                *sel = cur + 1;
            }
            sound_cursor_queue();
        }
    }
}

/* On the cancel button, flag the context's thread group as finished, turn every
 * THREAD_TASK_RESUME thread into a stop request and report -1 to the caller. Closing the
 * battle menu slot also forgets the pending selection. Menu input is then held off for 5
 * frames. */
void menu_handle_cancel_input(menu_entry_t* context) {
    s32 i;
    s32 j;
    s32 thread_id;
    s32 count;
    s16* result;

    if (!(g_script_event_input & PSX_PAD_CROSS)) {
        return;
    }
    if (menu_can_accept_input() == 0) {
        return;
    }
    count = context->thread_count;
    if (count != -1) {
        for (i = 0; i < count; i++) {
            thread_id = i + g_thread_current_id;
            g_thread_contexts[thread_id].function_parameter_3 = 1;
            if (thread_id == THREAD_SLOT_MENU) {
                for (j = 3; j >= 0; j--) {
                    g_command_pending_selection[j] = -1;
                }
            }
        }
        for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
            if (g_thread_contexts[i].task_id == THREAD_TASK_RESUME) {
                g_thread_contexts[i].task_id = THREAD_TASK_STOP_REQUEST;
            }
        }
    }
    g_script_event_input = 0;
    result = context->value;
    *result = -1;
    sound_cancel_queue();
    g_script_input_suppression_frames = 5;
}

/* Suppress menu input while another thread has a stop request and report
 * completion: 1 once no other running thread has a stop request and this
 * thread's parameter 3 is set. In WORLD, SELECT first opens the entry's
 * auxiliary text thread when its table selector is nonzero. */
s32 menu_should_close_thread(u32* input) {
#ifdef FFT_WORLD
    menu_entry_t* parameter;
#endif
    s32 i;

#ifdef FFT_WORLD
    if (g_thread_contexts[THREAD_SCREEN_SLOT_TEXT].is_running == 0 && menu_can_accept_input() != 0
        && (*input & PSX_PAD_SELECT)) {
        parameter = (menu_entry_t*)thread_current_parameter_1_get();
        if (parameter->select_text_table != 0) {
            if (g_script_input_suppression_frames == 0) {
                thread_start(THREAD_SCREEN_SLOT_TEXT, screen_help_text_thread);
                thread_parameters_set(THREAD_SCREEN_SLOT_TEXT, (s32)parameter, 0, 0);
                g_menu_input_lock_mode = 1;
            }
            *input = 0;
        }
    }
#endif
    if (g_menu_input_lock_mode != 0 || g_help_open != 0) {
        *input = 0;
        return 0;
    }
    for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
        if (i != g_thread_current_id && thread_is_running_asm(i) != 0
            && g_thread_contexts[i].task_id == THREAD_TASK_STOP_REQUEST) {
            *input = 0;
            break;
        }
    }
    if (i == VARIANT_THREAD_TABLE_SLOT_COUNT && thread_current_parameter_3_get() != 0) {
        return 1;
    }
    return 0;
}
