#include "fft/event.h"
#include "fft/world.h"
#include "psx/pad.h"

void world_menu_confirm_item_purchase(void) {
    s32 result = 1;

    if (g_world_shop_purchase_message_wait != 0) {
        if (g_world_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_purchase_message_wait = 0;
            world_thread_set_parameters(2, 0x19, 0xF805, 0);
            g_world_menu_prompt_active = 0;
            g_world_shop_menu_step = 1;
        }
    } else {
        result = world_shop_run_item_purchase_quantity_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_obtain_gil(-world_item_get_price(g_world_menu_entry_ids[g_world_menu_cursor_position])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_item_change_quantity_on_equip(g_world_menu_entry_ids[g_world_menu_cursor_position],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_thread_set_parameters(2, 0x19, 0xF811, 0);
            world_item_reconcile_sorted_list(g_world_shop_item_category);
        } else {
            world_thread_set_parameters(2, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_purchase_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    world_menu_run_script_with_palette_mode(g_world_shop_purchase_menu_script, 0, g_world_thread_task_active);
    world_menu_draw_pressable_button(3, 0xBA, 0x4C, 0, 1, 1);
    world_shop_run_obtain_gil_menu();
}
