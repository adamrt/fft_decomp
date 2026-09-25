#include "fft/world.h"
#include "psx/types.h"

/* Fur Shop buy-back twin of world_shop_run_item_sell_menu: the quantity prompt
 * is capped by the 99-item inventory limit, the gil on hand at half price
 * and the poached stock. */
s32 world_shop_run_item_repurchase_menu(void) {
    s32 owned;
    s32 count;
    s32 price;
    s32 room;
    s32 stock;

    if (g_world_shop_repurchase_result == 0) {
        g_world_shop_repurchase_confirm_shown = 0;
        g_world_shop_repurchase_item = *(u16*)&g_world_menu_entry_ids[g_world_menu_cursor_position];
        g_world_menu_prompt_active = 1;
        owned = world_item_count_owned_and_equipped((s16)g_world_shop_repurchase_item);
        if (owned >= 99) {
            world_thread_set_parameters(2, 0x19, 0xF834, 0);
            return -2;
        }
        count = world_shop_obtain_gil(0);
        price = world_item_get_price((s16)g_world_shop_repurchase_item) >> 1;
        count /= price;
        g_world_shop_repurchase_price = price;
        room = 99 - owned;
        if (room < count) {
            count = room;
        }
        if (count == 0) {
            world_thread_set_parameters(2, 0x19, 0xF833, 0);
            return -1;
        }
        world_thread_set_parameters(2, 0x19, 0xF835, 0);
        stock = world_shop_adjust_poached_item_quantity((s16)g_world_shop_repurchase_item, 0);
        if (count >= stock) {
            count = stock;
        }
        g_world_shop_quantity_limit = count;
        g_world_menu_sound_muted = 1;
    }
    g_world_shop_repurchase_result = world_menu_run_thread(0xF, &g_world_shop_quantity_menu_data);
    if (world_thread_is_running(0xE) != 0 && g_world_shop_repurchase_confirm_shown == 0) {
        g_world_text_substitution_values[0]
            = g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY] * g_world_shop_repurchase_price;
        world_thread_set_parameters(2, 0x19, 0xF836, 0);
        g_world_shop_repurchase_confirm_shown = 1;
        g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
    }
    if (world_thread_is_running(0xE) == 0 && g_world_shop_repurchase_confirm_shown != 0) {
        g_world_shop_repurchase_confirm_shown = 0;
        world_thread_set_parameters(2, 0x19, 0xF835, 0);
    }
    if (g_world_shop_repurchase_result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        }
        g_world_menu_sound_muted = 0;
    }
    return g_world_shop_repurchase_result;
}
