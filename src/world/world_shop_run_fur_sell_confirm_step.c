#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

/* Fur shop: confirm a sale. The party receives half price, and the sold copies join the
 * poached stock. */
void world_shop_run_fur_sell_confirm_step(void) {
    s32 result = 1;

    if (g_world_shop_fur_sale_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_fur_sale_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF841, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_SELL_LIST;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_fur_sell_quantity_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(world_item_get_half_price(g_item_list_entries[g_world_menu_cursor_position])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_world_menu_cursor_position],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_shop_adjust_poached_item_quantity(g_item_list_entries[g_world_menu_cursor_position],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF837, 0);
            item_rebuild_category_list(g_world_shop_item_category);
            world_item_build_category_list(0, g_world_shop_item_category + 1,
                g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            world_menu_init_and_load_scrollable_list(g_item_list_entries, g_world_menu_cursor_position,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_world_shop_sale_confirm_menu_script);
            world_menu_set_window_scale_step(10);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_fur_sale_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode(
        &g_world_shop_sale_confirm_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    world_shop_run_obtain_gil_menu();
}
