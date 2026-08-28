#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Handle input for the job-report confirmation level.
 *
 * The target keeps text ID 0xb81a and 0 in $a2/$a3 across the consecutive
 * world_thread_set_parameters and wldcore_window_build_yes_no_panel calls. The first callee only
 * reads those argument registers, so explicit register variables preserve the
 * observed calling sequence without emitting assembly. The two pending-result
 * calls also pass the values pointer even though the callee ignores it, so
 * they use a cast while retaining the callee's verified void prototype. */
void wldcore_list_handle_completed_propositions_input(s32* values) {
    s32 buttons;
    s32 depth;
    s32 index;
    s16* selection;
    s32 value;
    register s32 text_id __asm__("$6");
    register s32 zero __asm__("$7");

    if ((g_main_system_flags & 4) && world_thread_is_running(0xC) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_wldcore_menu_result = 0;
        g_main_system_flags ^= 4;
        g_wldcore_menu_stack_depth--;
        wldcore_menu_dispatch_resume_handler();
        return;
    }

    buttons = g_wldcore_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        wldcore_list_clear_cursor_state(4);
        g_main_system_flags |= 4;
        world_thread_set_parameters(0xC, 0, 0, 1);
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        ((void (*)(s32*))wldcore_list_completed_propositions_clear_result)(values);
        wldcore_menu_push_message_level(0x1061, 1);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        ((void (*)(s32*))wldcore_list_completed_propositions_clear_result)(values);
        selection = &g_wldcore_window_panel_render_state.selected_index;
        index = *selection;
        value = values[*selection + 13];
        wldcore_return_from_job_determinations(value);
        wldcore_list_store_cursor_state(4);
        g_world_text_substitution_values[0] = g_wldcore_list_entry_values[*selection];
        text_id = 0xB81A;
        zero = 0;
        world_thread_set_parameters(0xE, 0x19, text_id, zero);
        /* The origin is passed by value in a0/a1. */
        ((void (*)(s32, s32, s32, s32, s32, s32))wldcore_window_build_yes_no_panel)(
            0x48, 0x20, text_id, zero, 0x48, 0x20);
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS_BROWSE;
        g_wldcore_menu_stack_depth = depth + 1;
        g_wldcore_menu_stack_records_next[depth].job_report_confirm.argument = values[*selection + 13];
    }
}
