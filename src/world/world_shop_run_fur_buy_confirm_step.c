#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

/* Fur shop sibling of world_menu_confirm_item_purchase: the party pays half
 * price and the poached stock is reduced. */
void world_shop_run_fur_buy_confirm_step(void) {
    s32 result = 1;

    if (g_world_shop_fur_purchase_message_wait != 0) {
        if (g_world_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_fur_purchase_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF83B, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_LIST;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_fur_buy_quantity_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(-(world_item_get_price(g_world_menu_entry_ids[g_world_menu_cursor_position]) >> 1)
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_item_adjust_inventory_count(g_world_menu_entry_ids[g_world_menu_cursor_position],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_shop_adjust_poached_item_quantity(g_world_menu_entry_ids[g_world_menu_cursor_position],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_item_reconcile_sorted_list(g_world_shop_item_category);
            world_shop_build_item_list(-1, g_world_menu_context_id,
                g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_world_menu_entry_ids, ITEM_LIST_MODE_ALL);
            world_menu_init_and_load_scrollable_list(g_world_menu_entry_ids, g_world_menu_cursor_position,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_world_shop_fur_purchase_menu_script);
            world_menu_set_window_scale_step(10);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF837, 0);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_fur_purchase_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode(
        &g_world_shop_fur_purchase_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C, 0, 1, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
}
