#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Menu screen step: on the first frame reset the fade flag; once any of the
 * face buttons (0xf0) is pressed start the fade-out with a music fade, then
 * wait for the fade to complete before releasing the screen (g_world_shop_menu_step = -1). */
void world_menu_run_exit_fade_step(void) {
    if (g_world_shop_exit_step_initialized == 0) {
        g_world_shop_exit_fade_started = 0;
        g_world_shop_exit_step_initialized = 1;
    }
    if (g_world_shop_exit_fade_started != 0) {
        if (world_gfx_update_fade_out_tile() == 0) {
            g_world_shop_menu_step = -1;
            g_world_shop_exit_step_initialized = 0;
        }
    } else if (g_world_input_newly_pressed & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
        world_gfx_start_increasing_fade();
        main_sound_set_current_music_target(0, 0xF0);
        world_thread_set_parameters(2, 0x19, -1, 0);
        g_world_shop_exit_fade_started = 1;
    }
    world_shop_run_obtain_gil_menu();
    if (g_world_shop_id == 0x65) {
        world_shop_run_soldier_office_fee_menu_script();
        world_menu_run_script_with_callback_suppressed();
    }
}
