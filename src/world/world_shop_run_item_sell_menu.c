#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Shop: the quantity prompt of a sale, capped by the inventory count.
 *
 * The first call returns -1 with message 0xF80E when none of the highlighted
 * item is in the inventory. Otherwise it returns the quantity menu's thread
 * state, showing the total (half price times quantity, message 0xF810) while
 * the confirmation child runs and the plain prompt (0xF80F) otherwise. The
 * final frame queues the transaction sound for "yes" and the confirm sound
 * for "no". */
s32 world_shop_run_item_sell_menu(void) {
    s32 count;

    if (g_world_shop_sell_result == 0) {
        g_world_shop_sell_confirm_shown = 0;
        g_world_shop_sell_item = g_item_list_entries[g_world_menu_cursor_position];
        g_world_menu_prompt_active = 1;
        count = item_adjust_inventory_count(g_world_shop_sell_item, 0);
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80E, 0);
            return -1;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80F, 0);
        g_world_shop_quantity_limit = count;
        g_world_shop_sell_confirm_shown = 0;
        g_sound_suppressed = 1;
    }
    g_world_shop_sell_result = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_quantity_menu_data);
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_world_shop_sell_confirm_shown == 0) {
        g_text_substitution_values[0] = world_item_get_half_price(g_world_shop_sell_item)
            * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY];
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF810, 0);
        g_world_shop_sell_confirm_shown = 1;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_world_shop_sell_confirm_shown != 0) {
        g_world_shop_sell_confirm_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80F, 0);
    }
    if (g_world_shop_sell_result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_world_shop_sell_result;
}
