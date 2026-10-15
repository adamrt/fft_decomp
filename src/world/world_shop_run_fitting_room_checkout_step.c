#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Fitting room step that prices the tried-on equipment.
 *
 * Runs the checkout prompt: done returns to the shop's main menu (message
 * 0xf802); canceled shows message 0xf818 until any face button, which
 * returns to the fitting room with the unit's stats recalculated.
 */
void world_shop_run_fitting_room_checkout_step(void) {
    s32 result;

    g_world_shop_background_visible = 1;
    if (g_world_shop_checkout_complete != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_world_shop_checkout_complete = 0;
            g_world_formation_unit_banner_enabled = 1;
            world_formation_recalculate_selected_unit_stats();
        }
    } else {
        result = world_shop_run_fitting_room_checkout_prompt();
        if (result == WORLD_SHOP_CHECKOUT_DONE) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_MAIN_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf802, 0);
            g_world_shop_fitting_room_initialized = 0;
        } else if (result == WORLD_SHOP_CHECKOUT_CANCELED) {
            g_world_shop_checkout_complete = 1;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf818, 0);
        }
    }
    world_shop_run_obtain_gil_menu();
}
