#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Fur shop twin of world_shop_run_item_purchase_quantity_menu: the quantity
 * prompt is capped by the 99-item inventory limit, the gil on hand at half
 * price and the poached stock. */
s32 world_shop_run_fur_buy_quantity_menu(void) {
    s32 owned;
    s32 count;
    s32 price;
    s32 room;
    s32 stock;

    if (g_world_shop_fur_purchase_result == 0) {
        g_world_shop_fur_purchase_confirm_shown = 0;
        g_world_shop_fur_purchase_item = g_world_menu_entry_ids[g_world_menu_cursor_position];
        g_world_menu_prompt_active = 1;
        owned = world_item_count_owned_and_equipped(g_world_shop_fur_purchase_item);
        if (owned >= 99) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF834, 0);
            return WORLD_SHOP_QUANTITY_HOLDING_MAX;
        }
        count = world_shop_adjust_gil(0);
        price = world_item_get_price(g_world_shop_fur_purchase_item) >> 1;
        count /= price;
        g_world_shop_fur_purchase_price = price;
        room = 99 - owned;
        if (room < count) {
            count = room;
        }
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF833, 0);
            return WORLD_SHOP_QUANTITY_CANNOT_AFFORD;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF835, 0);
        stock = world_shop_adjust_poached_item_quantity(g_world_shop_fur_purchase_item, 0);
        if (count >= stock) {
            count = stock;
        }
        g_world_shop_quantity_limit = count;
        g_sound_suppressed = 1;
    }
    g_world_shop_fur_purchase_result
        = world_menu_run_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_quantity_menu_data);
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_world_shop_fur_purchase_confirm_shown == 0) {
        g_text_substitution_values[0]
            = g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY] * g_world_shop_fur_purchase_price;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF836, 0);
        g_world_shop_fur_purchase_confirm_shown = 1;
        g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
    }
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_world_shop_fur_purchase_confirm_shown != 0) {
        g_world_shop_fur_purchase_confirm_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF835, 0);
    }
    if (g_world_shop_fur_purchase_result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_world_shop_fur_purchase_result;
}
