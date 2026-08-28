#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

void world_shop_run_recruit_unit_preview_step(void) {
    if (g_world_shop_recruit_preview_initialized == 0) {
        if (g_world_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            world_thread_set_parameters(2, 0x19, -1, 0);
        }
        if (world_thread_is_running(2) == 0) {
            g_world_formation_unit_browse_enabled = 1;
            g_world_formation_unit_banner_enabled = 1;
            g_world_formation_scroll_enabled = 0;
            g_world_status_display_thread_params.y = 0;
            g_world_menu_description_text_id = 0x20003;
            g_world_shop_recruit_preview_initialized = 1;
        }
        world_shop_run_obtain_gil_menu();
        world_shop_run_soldier_office_fee_menu_script();
        world_menu_run_script_with_callback_suppressed();
    } else if (g_world_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
        g_world_shop_menu_step = 0x16;
        g_world_formation_unit_browse_enabled = 0;
        g_world_formation_unit_banner_enabled = 0;
        world_menu_toggle_numeric_editor_thread(0);
        world_menu_stop_unit_status_banner_thread(9);
        world_thread_start(2, world_text_message_box_thread);
        world_thread_set_parameters(2, 0x19, 0xF826, 0);
        g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        g_world_shop_recruit_preview_initialized = 0;
    }
}
