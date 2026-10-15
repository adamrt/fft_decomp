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

void world_shop_run_fur_menu_step(void) {
    s32 i;

    if (g_world_shop_fur_opened == 0) {
        g_menu_list_selected_index = 0;
        menu_set_vsync_mode_and_event_speed(0);
        g_world_shop_fur_has_secret_hunt = 0;
        g_world_shop_fur_has_poached_stock = 0;
        g_world_shop_fur_access_granted = 0;
        g_world_shop_fur_waiting_message = 0;
        for (i = 0; i < g_world_formation_unit_count; i++) {
            if (world_ability_has_secret_hunt(i) != 0) {
                g_world_shop_fur_has_secret_hunt = 1;
            }
        }
        if (g_world_shop_fur_has_secret_hunt != 0) {
            for (i = 1; i < 0x100; i++) {
                if (world_shop_adjust_poached_item_quantity(i, 0) != 0) {
                    g_world_shop_fur_has_poached_stock = 1;
                }
            }
        }
        if (g_world_shop_fur_has_secret_hunt == 0) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82D, 0);
            return;
        }
        g_world_shop_fur_access_granted = 1;
        g_world_shop_fur_opened = 1;
    }
    if (g_world_shop_fur_waiting_message != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF830, 0);
            g_world_shop_fur_waiting_message = 0;
        }
    } else if (menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_shop_fur_menu_thread_data) == 0) {
        menu_clear_selection_records();
        if (g_world_menu_selection_results[0] == 0) {
            if (g_world_shop_fur_has_poached_stock != 0) {
                g_world_shop_item_category = menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
                g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_LIST;
                g_world_shop_fur_opened = 0;
            } else {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82E, 0);
                g_world_shop_fur_waiting_message = 1;
            }
        } else if (g_world_menu_selection_results[0] == 1) {
            if (world_menu_has_items(0) != 0) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_SELL_LIST;
                g_world_shop_fur_opened = 0;
            } else {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF845, 0);
                g_world_shop_fur_waiting_message = 1;
            }
        } else {
            g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF831, 0);
            g_world_shop_fur_opened = 0;
        }
    }
    world_shop_run_obtain_gil_menu();
}
