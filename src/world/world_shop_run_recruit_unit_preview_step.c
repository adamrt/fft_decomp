#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

/*
 * Soldier office step that shows the generated recruit.
 *
 * Any face button dismisses the message box; once it has closed, unit
 * browsing and the status banner are turned on for the preview. A further
 * face button closes the panels and moves on to the hire confirmation
 * (message 0xF826).
 */
void world_shop_run_recruit_unit_preview_step(void) {
    if (g_world_shop_recruit_preview_initialized == 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_world_formation_unit_browse_enabled = 1;
            g_world_formation_unit_banner_enabled = 1;
            g_world_formation_unit_banner_at_bottom = 0;
            g_menu_status_display_thread_params.y = 0;
            g_menu_help_message_text_id = MENU_GRID_HELP_RECRUIT_PREVIEW;
            g_world_shop_recruit_preview_initialized = 1;
        }
        world_shop_run_obtain_gil_menu();
        world_shop_run_soldier_office_fee_menu_script();
        world_menu_run_script_with_callback_suppressed();
    } else if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
        g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_CONFIRM;
        g_world_formation_unit_browse_enabled = 0;
        g_world_formation_unit_banner_enabled = 0;
        menu_toggle_numeric_editor_thread(0);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF826, 0);
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        g_world_shop_recruit_preview_initialized = 0;
    }
}
