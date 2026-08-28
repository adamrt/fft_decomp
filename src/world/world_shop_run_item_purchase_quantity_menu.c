#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"

/* Runs the shop purchase-quantity menu for the highlighted item.
 *
 * On entry, caps the selectable quantity at min(gil / price, 99 - owned) and
 * returns -2 when the stack is already full or -1 when no unit is
 * affordable. Afterwards it returns the quantity menu thread state, updating
 * the total-price prompt while the confirmation sub-thread is running. */
s32 world_shop_run_item_purchase_quantity_menu(void) {
    s32 owned;
    s32 count;
    s32 limit;

    if (g_world_shop_quantity_menu_running == 0) {
        g_world_shop_purchase_total_shown = 0;
        g_world_menu_prompt_active = 1;
        g_world_shop_purchase_item = g_world_menu_entry_ids[g_world_menu_cursor_position];
        owned = world_item_count_owned_and_equipped(g_world_shop_purchase_item);
        if (owned >= 99) {
            world_thread_set_parameters(2, 0x19, 0xF807, 0);
            return -2;
        }
        count = world_shop_obtain_gil(0);
        g_world_shop_purchase_unit_price = world_item_get_price(g_world_shop_purchase_item);
        count /= g_world_shop_purchase_unit_price;
        limit = 99 - owned;
        if (limit < count) {
            count = limit;
        }
        if (count == 0) {
            world_thread_set_parameters(2, 0x19, 0xF806, 0);
            return -1;
        }
        world_thread_set_parameters(2, 0x19, 0xF808, 0);
        g_world_shop_quantity_limit = count;
        g_world_shop_purchase_total_shown = 0;
        g_world_menu_sound_muted = 1;
    }
    g_world_shop_quantity_menu_running = world_menu_run_thread(0xF, &g_world_shop_quantity_menu_data);
    if (world_thread_is_running(0xE) != 0 && g_world_shop_purchase_total_shown == 0) {
        g_world_text_substitution_values[0]
            = g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY] * g_world_shop_purchase_unit_price;
        world_thread_set_parameters(2, 0x19, 0xF809, 0);
        g_world_shop_purchase_total_shown = 1;
        g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
    }
    if (world_thread_is_running(0xE) == 0 && g_world_shop_purchase_total_shown != 0) {
        g_world_shop_purchase_total_shown = 0;
        world_thread_set_parameters(2, 0x19, 0xF808, 0);
    }
    if (g_world_shop_quantity_menu_running == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        }
        g_world_menu_sound_muted = 0;
    }
    return g_world_shop_quantity_menu_running;
}
