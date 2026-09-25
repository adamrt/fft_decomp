#include "fft/wldcore.h"
#include "psx/pad.h"

/* Poll the confirm/cancel buttons for the world-map unit browser panel and
 * tear it down once the panel's render thread has finished.
 *
 * level->result is 0 while waiting, 1 on confirm and -1 on cancel. */
void wldcore_menu_step_unit_browser_panel(wldcore_menu_panel_level_t* level) {
    u32 buttons;

    if ((level->result != 0) && (world_thread_is_running(0xC) == 0)) {
        g_wldcore_menu_result = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_wldcore_menu_stack_depth--;
        if (level->result == 1) {
            if (g_wldcore_active_menu_value < 0x200) {
                main_entd_init_event_unit_data(g_wldcore_active_menu_value);
                world_text_generate_formation_unit_name_string();
                g_main_system_flags |= 2;
            }
        }
        wldcore_menu_dispatch_resume_handler();
        return;
    }

    buttons = g_wldcore_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = -1;
        world_thread_set_parameters(0xC, 0, 0, 1);
    } else if ((buttons & PSX_PAD_CIRCLE) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        world_thread_set_parameters(0xC, 0, 0, 1);
    }
}
