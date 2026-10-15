#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Shop main menu step: runs the Buy / Sell / Fitting Room menu and, when it
 * closes, moves to the chosen step (rows 0..2; the fitting room also sets its
 * message), or to the exit fade with its message for anything else. */
void world_shop_run_main_menu_step(void) {
    u8 running;

    if (g_world_shop_main_menu_running == 0) {
        g_world_formation_unit_cycle_mode = 0;
        g_menu_list_selected_index = 0;
    }
    running = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_shop_main_menu_thread_data);
    g_world_shop_main_menu_running = running;
    if (running == 0) {
        menu_clear_selection_records();
        if (g_world_menu_selection_results[0] == 0) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_LIST;
            g_world_shop_item_category = menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
        } else if (g_world_menu_selection_results[0] == 1) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_SELL_LIST;
        } else if (g_world_menu_selection_results[0] == 2) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
            g_unit_selected_index = 0;
            world_formation_stage_selected_unit();
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF818, 0);
        } else {
            g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF803, 0);
        }
    }
    world_shop_run_obtain_gil_menu();
}
