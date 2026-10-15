#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Menu input step: cancel (0x40) closes this level and returns through
 * 0x80072a18; confirm (0x20) stores the 0x8009f2f4 + 0x8009f2f6 total, shows
 * message 0xb810 (with the total substituted) or 0xb80f when it is zero,
 * opens the yes/no panel at (0x48, 0x20), and pushes a level tagged 0xa. */
void wldcore_proposition_handle_accept_input(wldcore_window_render_pair_t* owner) {
    s32 total;
    point32_t origin;

    if (g_wldcore_window_render_records[owner->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count--;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_available_propositions();
    } else if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        total = g_wldcore_selected_proposition_row[0].fee_a + g_wldcore_selected_proposition_row[0].fee_b;
        g_wldcore_proposition_gil_amount = total;
        if (total != 0) {
            g_text_substitution_values[0] = total;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOB_FEE_PROMPT, 0);
        } else {
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_ACCEPT_JOB_PROMPT, 0);
        }
        wldcore_window_set_render_palette_2(owner);
        origin.x = 0x48;
        origin.y = 0x20;
        wldcore_window_build_yes_no_panel(origin);
        g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_ACCEPT;
        g_wldcore_menu_stack_depth++;
    }
}
