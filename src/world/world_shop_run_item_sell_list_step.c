#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Item-list shop menu step: on first entry prompts 0xF80B (0xF80C when the
 * player already holds items), rebuilds the item list and installs the seven
 * column callbacks, then handles cancel (0x40) and confirm (0x20).
 */
void world_shop_run_item_sell_list_step(void) {
    if (g_world_shop_sell_list_initialized == 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, world_menu_has_items(0) != 0 ? 0xF80C : 0xF80B, 0);
        g_world_shop_item_category = menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
        world_item_build_category_list(-1, g_world_shop_item_category + 1,
            g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_init_scrollable_list_core_variant(
            g_item_list_entries, g_menu_list_selected_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
        g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
        g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4].value = world_item_get_selected_half_price;
        g_world_menu_script_callbacks[5].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[6].value = menu_get_entry_item_count;
        g_world_shop_sell_list_initialized = 1;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_world_shop_sell_list_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF802, 0);
        g_world_shop_menu_step = WORLD_SHOP_STEP_MAIN_MENU;
    } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_menu_list_entry_count == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_world_shop_quantity_menu_data[1].selected_index = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_SELL_CONFIRM;
        }
    }
    world_menu_switch_item_category(g_menu_input_secondary_repeat);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_sell_list_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    world_shop_run_obtain_gil_menu();
}
