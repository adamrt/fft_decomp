#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Item sell confirmation step: pays half price times the quantity, removes the
 * items, prompts 0xF811 and rebuilds the list (0xF80A when nothing to sell).
 */
void world_shop_run_item_sell_confirm_step(void) {
    s32 result = 1;
    u8 category;
    s32 next_category;
    s32 message;

    if (g_world_shop_sell_message_wait != 0) {
        if (g_world_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_sell_message_wait = 0;
            world_thread_set_parameters(2, 0x19, 0xF80D, 0);
            g_world_shop_menu_step = 4;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_item_sell_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_obtain_gil(world_item_get_half_price(g_world_menu_entry_ids[g_world_menu_cursor_position])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_item_change_quantity_on_equip(g_world_menu_entry_ids[g_world_menu_cursor_position],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_thread_set_parameters(2, 0x19, 0xF811, 0);
            world_item_reconcile_sorted_list(g_world_shop_item_category);
            category = g_world_shop_item_category;
            next_category = category + 1;
            if (g_world_shop_item_category != 4) {
                message = (s8)(category + 1);
            } else {
                message = 7;
            }
            world_item_build_category_list(
                0, next_category, message, (world_item_list_entry_t*)g_world_menu_entry_ids, 0);
            world_menu_init_and_load_scrollable_list(g_world_menu_entry_ids, g_world_menu_cursor_position,
                g_world_text_item_names, g_world_shop_sell_list_menu_script);
            world_menu_set_window_scale_step(10);
        } else {
            world_thread_set_parameters(2, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_sell_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    world_menu_run_script_with_palette_mode(g_world_shop_sell_list_menu_script, 0, world_thread_is_running(1));
    world_shop_run_obtain_gil_menu();
}
