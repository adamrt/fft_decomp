#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

extern s16 g_world_menu_pending_selection[];

/* Take the confirmed action for menu row row_index: start the linked menu
 * thread, or close the requested number of menu levels, then store the row as
 * the menu's selection.
 *
 * WORLD analog of battle_menu_handle_action. */
void world_menu_handle_entry_confirm(world_menu_entry_t* entry, s32 row_index) {
    s32 silent;
    s32 target;
    s32 i;
    s32 j;
    s32 thread_id;
    s32 menu_thread_id;
    s16* parent_indices;

    if (g_world_menu_input_disabled == 2
        && ((world_script_get_variable(EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS) >> row_index) & 1)) {
        return;
    }
    silent = 0;
    if ((g_world_menu_new_button_input & PSX_PAD_CIRCLE) && world_menu_is_input_allowed() != 0) {
        if (g_world_thread_contexts[g_world_thread_current_id].task_words[1] == 0) {
            g_world_thread_contexts[g_world_thread_current_id].task_words[1] = 2;
            g_world_thread_contexts[g_world_thread_current_id].task_words[0] = 0;
        }
        parent_indices = entry->parent_indices;
        target = ((u32)((s32)parent_indices + 0xf) < 0xf) ? (s32)parent_indices : parent_indices[row_index];
        if (target >= 0) {
            if (target & 0x1000) {
                silent = 1;
                /* Raises silent's allocation priority; otherwise $s6-$s8 rotate. */
                __asm__("" : : "r"(silent), "r"(silent));
                target -= 0x1000;
            }
        }
        if (target >= 0) {
            if (target < 0x100) {
                world_thread_start(g_world_thread_current_id - 1, g_world_menu_thread_menu_data[target].thread_entry);
                world_thread_set_parameters(
                    g_world_thread_current_id - 1, (s32)&g_world_menu_thread_menu_data[target], 0, 0);
            } else if (target == 0x4d2) {
                row_index = -1;
            }
        } else {
            target = -target;
            for (i = 0; i < 17; i++) {
                if (g_world_thread_contexts[i].task_id == NATIVE_THREAD_TASK_RESUME) {
                    g_world_thread_contexts[i].task_id = NATIVE_THREAD_TASK_STOP_REQUEST;
                }
            }
            for (i = 0; i < target; i++) {
                thread_id = i + g_world_thread_current_id;
                g_world_thread_contexts[thread_id].function_parameter_3 = 1;
                if (thread_id == 8) {
                    for (j = 0; j < target; j++) {
                        menu_thread_id = 8 - j;
                        if (world_thread_is_running_80100164(menu_thread_id) != 0) {
                            g_world_menu_pending_selection[j]
                                = ((world_menu_entry_t*)g_world_thread_contexts[menu_thread_id].function_parameter_1)
                                      ->selected_index;
                        }
                    }
                }
            }
        }
        g_world_menu_new_button_input = 0;
        *entry->value = row_index;
        if (g_world_menu_sound_muted == 0 && silent == 0) {
            world_sound_set_effect_to_confirm();
        }
    }
}
