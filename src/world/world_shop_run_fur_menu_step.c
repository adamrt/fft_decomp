#include "fft/data.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Fur shop menu step: on entry checks for a Secret Hunt unit and poached
 * stock (refusing entry with message 0xf82d when nobody has the ability),
 * then routes the selected entry to the buy-back menu, the item menu or a
 * refusal message. */
void world_shop_run_fur_menu_step(void) {
    s32 i;

    if (g_world_shop_fur_opened == 0) {
        g_world_menu_cursor_position = 0;
        world_script_set_vsync_mode_and_event_speed(0);
        g_world_shop_fur_has_secret_hunt = 0;
        g_world_shop_fur_has_poached_stock = 0;
        g_world_shop_fur_access_granted = 0;
        g_world_shop_fur_waiting_message = 0;
        for (i = 0; i < g_world_formation_unit_count; i++) {
            /* The target passes i without the definition's s16 narrowing. */
            if (((s32 (*)(s32))world_ability_has_secret_hunt)(i) != 0) {
                g_world_shop_fur_has_secret_hunt = 1;
            }
        }
        if (g_world_shop_fur_has_secret_hunt != 0) {
            for (i = 1; i < 0x100; i++) {
                if (world_shop_adjust_poached_item_quantity((s16)i, 0) != 0) {
                    g_world_shop_fur_has_poached_stock = 1;
                }
            }
        }
        if (g_world_shop_fur_has_secret_hunt == 0) {
            g_world_shop_menu_step = 0xD;
            world_thread_set_parameters(2, 0x19, 0xF82D, 0);
            return;
        }
        g_world_shop_fur_access_granted = 1;
        g_world_shop_fur_opened = 1;
    }
    if (g_world_shop_fur_waiting_message != 0) {
        if (g_world_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            world_thread_set_parameters(2, 0x19, 0xF830, 0);
            g_world_shop_fur_waiting_message = 0;
        }
    } else if (world_menu_run_thread(6, &g_world_shop_fur_menu_thread_data) == 0) {
        world_gfx_clear_sprite_slots();
        if (g_world_menu_selection_results[0] == 0) {
            if (g_world_shop_fur_has_poached_stock != 0) {
                g_world_shop_item_category = world_menu_set_cursor_position_2(0, 0);
                g_world_shop_menu_step = 0xF;
                g_world_shop_fur_opened = 0;
            } else {
                world_thread_set_parameters(2, 0x19, 0xF82E, 0);
                g_world_shop_fur_waiting_message = 1;
            }
        } else if (g_world_menu_selection_results[0] == 1) {
            if (world_menu_has_items(0) != 0) {
                g_world_shop_menu_step = 0x12;
                g_world_shop_fur_opened = 0;
            } else {
                world_thread_set_parameters(2, 0x19, 0xF845, 0);
                g_world_shop_fur_waiting_message = 1;
            }
        } else {
            g_world_shop_menu_step = 0xD;
            world_thread_set_parameters(2, 0x19, 0xF831, 0);
            g_world_shop_fur_opened = 0;
        }
    }
    world_shop_run_obtain_gil_menu();
}
