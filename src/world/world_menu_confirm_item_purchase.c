#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

/*
 * Shop buy step: runs the purchase-quantity prompt for the highlighted item
 * and completes the purchase.
 *
 * On "yes" the gil is deducted (price times quantity), the items are added to
 * the inventory and message 0xF811 is shown; on "no" message 0xF80A. That
 * message, or the prompt's refusal (stack full or not affordable), stays
 * until any face button or d-pad press, which returns to the buy list. The
 * list, the Triangle button and the gil display are drawn every frame.
 */
void world_menu_confirm_item_purchase(void) {
    s32 result = 1;

    if (g_world_shop_purchase_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_purchase_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF805, 0);
            g_world_menu_prompt_active = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_LIST;
        }
    } else {
        result = world_shop_run_item_purchase_quantity_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(-world_item_get_price(g_item_list_entries[g_menu_list_selected_index])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF811, 0);
            item_rebuild_category_list(g_world_shop_item_category);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_purchase_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode_variant(&g_world_shop_purchase_menu_script, 0, g_menu_input_lock_mode);
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C, 0, 1, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
}
