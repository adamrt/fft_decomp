#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

/* Same address as g_main_system_flags (0x8004d950). The target re-reads the
 * flags in both arms after the (flags & 0xc) == 4 test; reading the test
 * through this second link name keeps GCC from reusing the tested load, where
 * a volatile declaration also reschedules the argument setup. */
extern s32 g_main_system_flags_alias;

void wldcore_list_tutorial_mask_1_clear_result(void);

/* Input step of a list level that hands the chosen entry to the world script.
 *
 * State 2 waits for thread 14, then marks the selected entry in tutorial
 * progress mask 1, sets script variables 0x1fc and 0x27 and requests the event
 * through system flag 0x200000. While flags & 0xc is 4, a confirmed choice
 * starts thread 14 (state 2) and a cancelled one pops the level. Otherwise
 * cancel (0x40) and confirm (0x20) start thread 12 and record the choice;
 * 0x100 on a non-empty list opens help message 0x108c. */
void wldcore_list_handle_tutorial_mask_1_entries_input(wldcore_menu_panel_level_t* level) {
    s32 state;
    s16* selected;

    state = level->result;
    if (state == 2) {
        if (world_thread_is_running(0xE) == 0) {
            selected = &g_wldcore_window_panel_render_state.selected_index;
            wldcore_set_bit_value(&g_main_tutorial_progress_masks[1], *selected, 1);
            world_script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 1);
            world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_tutorial_event_ids[*selected]);
            wldcore_list_store_cursor_state(0x13);
            g_main_saved_list_cursor_state = g_wldcore_tutorial_entries_cursor_state;
            g_main_system_flags = (g_main_system_flags | 0x200000) ^ 1;
            g_main_saved_weather_variable = world_script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        }
        return;
    }
    if ((g_main_system_flags_alias & 0xC) == 4) {
        if (state != 0) {
            g_main_system_flags |= 0x40;
            world_thread_set_parameters(0xE, 0, -1, 0);
            level->result = 2;
            return;
        }
        g_wldcore_menu_ordering_table_offset = 1;
        g_wldcore_menu_result = 0;
        g_main_system_flags ^= 4;
        g_wldcore_menu_stack_depth--;
        wldcore_list_clear_cursor_state(0x13);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    } else if ((g_wldcore_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        /* The target passes level even though this callee ignores it. */
        ((void (*)(wldcore_menu_panel_level_t*))wldcore_list_tutorial_mask_1_clear_result)(level);
        wldcore_menu_push_message_level(0x108C, 1);
    } else if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        g_main_system_flags |= 4;
        world_thread_set_parameters(0xC, 0, 0, 1);
        wldcore_fade_start_screen(2, 0x10);
    }
}
