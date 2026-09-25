#include "fft/world.h"
#include "psx/types.h"

s32 world_shop_run_item_sell_menu_2(void) {
    s32 count;

    if (g_world_shop_fur_quantity_menu_running == 0) {
        g_world_shop_sell_total_shown = 0;
        g_world_shop_sell_quantity_item = *(u16*)&g_world_menu_entry_ids[g_world_menu_cursor_position];
        g_world_menu_prompt_active = 1;
        count = world_item_change_quantity_on_equip((s16)g_world_shop_sell_quantity_item, 0);
        if (count == 0) {
            world_thread_set_parameters(2, 0x19, 0xF842, 0);
            return -1;
        }
        world_thread_set_parameters(2, 0x19, 0xF843, 0);
        g_world_shop_quantity_limit = count;
        g_world_shop_sell_total_shown = 0;
        g_world_menu_sound_muted = 1;
    }
    g_world_shop_fur_quantity_menu_running = world_menu_run_thread(0xF, &g_world_shop_quantity_menu_data);
    if (world_thread_is_running(0xE) != 0 && g_world_shop_sell_total_shown == 0) {
        g_world_text_substitution_values[0] = world_item_get_half_price((s16)g_world_shop_sell_quantity_item)
            * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY];
        world_thread_set_parameters(2, 0x19, 0xF844, 0);
        g_world_shop_sell_total_shown = 1;
        g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
    }
    if (world_thread_is_running(0xE) == 0 && g_world_shop_sell_total_shown != 0) {
        g_world_shop_sell_total_shown = 0;
        world_thread_set_parameters(2, 0x19, 0xF843, 0);
    }
    if (g_world_shop_fur_quantity_menu_running == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        }
        g_world_menu_sound_muted = 0;
    }
    return g_world_shop_fur_quantity_menu_running;
}
