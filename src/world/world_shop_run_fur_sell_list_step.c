#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Fur shop: the list of the party's own inventory to sell; sibling of
 * world_shop_run_fur_buy_list_step.
 *
 * The callback block is written in ascending address order (794 -> 7ac), as
 * the sibling writes it. That makes gcc materialise the two twice-used
 * addresses (count_2 in a0, count in v1) up front at 0xb0-0xbc and defer
 * their stores; the target's emission order then follows. First-use order,
 * not store order, decides this.
 */
void world_shop_run_fur_sell_list_step(void) {
    s32 message;

    if (g_world_shop_fur_sell_list_initialized == 0) {
        message = world_menu_has_items(0) != 0 ? 0xf840 : 0xf845;

        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, message, 0);
        g_world_shop_item_category = world_menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
        world_item_build_category_list(-1, g_world_shop_item_category + 1,
            g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        world_menu_init_scrollable_list_core(g_item_list_entries, 0, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
        g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
        g_world_menu_script_callbacks[3].icon = world_menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4].value = world_item_get_selected_half_price;
        g_world_menu_script_callbacks[5].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[6].value = menu_get_entry_item_count;
        g_world_shop_fur_sell_list_initialized = 1;
    }

    menu_set_vsync_mode_and_event_speed(2);
    if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) != 0) {
        g_world_shop_fur_sell_list_initialized = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf830, 0);
        g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_MENU;
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
    } else if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) != 0) {
        if (g_world_menu_option_count == 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
        } else {
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_SELL_CONFIRM;
            g_world_shop_quantity_menu_data[1].selected_index = 0;
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        }
    }
    world_menu_switch_item_category(g_menu_input_newly_pressed);
    menu_run_script_with_palette_mode(
        &g_world_shop_sale_confirm_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    world_shop_run_obtain_gil_menu();
}
