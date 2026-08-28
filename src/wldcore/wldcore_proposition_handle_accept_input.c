#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

void wldcore_list_open_available_propositions(void);
void wldcore_window_build_yes_no_panel(wldcore_point32_t origin, s32 unused_2, s32 unused_3, s32 x2, s32 y2);

/* Menu input step: cancel (0x40) closes this level and returns through
 * 0x80072a18; confirm (0x20) stores the 0x8009f2f4 + 0x8009f2f6 total, shows
 * message 0xb810 (with the total substituted) or 0xb80f when it is zero,
 * opens the 0x80073778 panel, and pushes a level tagged 0xa.
 *
 * 0x80073778 reads only its first two arguments; the target passes the
 * (0x48, 0x20) pair again in stack slots 4-5 and leaves $a2/$a3 unset. The
 * uninitialised register variables reproduce that without extra moves. */
void wldcore_proposition_handle_accept_input(wldcore_menu_window_render_level_t* owner) {
    s32 total;
    /* Pinned: the call passes a2/a3 through unset. Unpinned, the
     * uninitialised locals get callee-saved registers and are copied in. */
    register s32 unused_2 __asm__("$6");
    register s32 unused_3 __asm__("$7");

    if (g_wldcore_window_render_records[owner->render_index].flags & 0x100) {
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count--;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_available_propositions();
    } else if (g_wldcore_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        total = g_wldcore_selected_proposition_row[0].fields.fee_a + g_wldcore_selected_proposition_row[0].fields.fee_b;
        g_wldcore_proposition_gil_amount = total;
        if (total != 0) {
            g_world_text_substitution_values[0] = total;
            world_thread_set_parameters(0xE, 0x19, 0xB810, 0);
        } else {
            world_thread_set_parameters(0xE, 0x19, 0xB80F, 0);
        }
        wldcore_window_set_render_state_2_from_arg_0x04((s32*)owner);
        /* The origin is passed by value in a0/a1. */
        ((void (*)(s32, s32, s32, s32, s32, s32))wldcore_window_build_yes_no_panel)(
            0x48, 0x20, unused_2, unused_3, 0x48, 0x20);
        g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_ACCEPT;
        g_wldcore_menu_stack_depth++;
    }
}
