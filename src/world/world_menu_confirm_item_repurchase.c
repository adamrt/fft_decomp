#include "fft/world.h"
#include "psx/pad.h"

/* Buy-back sibling of world_menu_confirm_item_purchase: the unit pays half
 * price and the poached count is reduced. */
void world_menu_confirm_item_repurchase(void) {
    s32 result = 1;
    s32 category;
    s32 item_list;

    if (g_world_shop_fur_purchase_message_wait != 0) {
        if (g_world_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_fur_purchase_message_wait = 0;
            world_thread_set_parameters(2, 0x19, 0xF83B, 0);
            g_world_shop_menu_step = 0xF;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_item_repurchase_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_obtain_gil(-(world_item_get_price(g_world_menu_entry_ids[g_world_menu_cursor_position]) >> 1)
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_item_change_quantity_on_equip((s16)g_world_menu_entry_ids[g_world_menu_cursor_position],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_shop_adjust_poached_item_quantity(g_world_menu_entry_ids[g_world_menu_cursor_position],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_item_reconcile_sorted_list(g_world_shop_item_category);
            item_list = g_world_shop_id;
            if (g_world_shop_item_category != 4) {
                /* The category is read again after the comparison. */
                __asm__ volatile("" ::: "memory");
                category = g_world_shop_item_category + 1;
            } else {
                category = 7;
            }
            world_shop_build_item_list(-1, item_list, category, (world_item_list_entry_t*)g_world_menu_entry_ids, 0);
            world_menu_init_and_load_scrollable_list(g_world_menu_entry_ids, g_world_menu_cursor_position,
                g_world_text_item_names, g_world_shop_fur_purchase_menu_script);
            world_menu_set_window_scale_step(10);
            world_thread_set_parameters(2, 0x19, 0xF837, 0);
        } else {
            world_thread_set_parameters(2, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_fur_purchase_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    world_menu_run_script_with_palette_mode(g_world_shop_fur_purchase_menu_script, 0, world_thread_is_running(1));
    world_menu_draw_pressable_button(3, 0xBA, 0x4C, 0, 1, 1);
    world_shop_run_obtain_gil_menu();
}
