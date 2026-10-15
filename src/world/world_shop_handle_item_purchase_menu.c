#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

/* Handle item selection and purchase-menu transitions.
 */
void world_shop_handle_item_purchase_menu(void) {
    s16 x;
    s16 y;
    s32 input;
    s32 disabled;
    s8 result;

    if (g_world_shop_purchase_list_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF804, 0);
        g_world_shop_purchase_list_initialized = 1;
        world_shop_build_item_list(-1, g_world_menu_context_id,
            g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        world_menu_get_selection_record(g_world_shop_item_category, &x, &y, g_item_list_entries);
        world_menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
        g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
        g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4].value = world_menu_get_item_price_display_value;
        g_world_menu_script_callbacks[5].value = world_item_count_selected_equipped_by_party;
        g_world_menu_script_callbacks[6].value = world_item_count_selected_owned_and_equipped;
        g_world_shop_purchase_equip_check_pending = 0;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (g_world_shop_purchase_equip_check_pending != 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        result = thread_is_running(THREAD_SCREEN_SLOT_MESSAGE);
        g_world_shop_purchase_equip_check_pending = result;
        if (result == 0) {
            g_world_shop_purchase_list_initialized = 0;
            menu_set_selection_record(g_world_shop_item_category, g_world_menu_cursor_position,
                g_world_menu_scroll_offset, g_item_list_entries);
            g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_EQUIP;
        }
        menu_input_clear_state();
    } else {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CROSS) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_shop_purchase_list_initialized = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_MAIN_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF802, 0);
        } else if ((input & PSX_PAD_TRIANGLE) && g_world_menu_option_count != 0 && g_world_shop_item_category != 4) {
            g_world_shop_purchase_equip_check_pending = 1;
        } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_world_menu_option_count == 0) {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_CONFIRM;
                g_world_shop_quantity_menu_data[1].selected_index = 0;
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            }
        }
    }
    disabled = 0;
    world_menu_switch_item_category(g_menu_input_secondary_repeat);
    menu_run_script_with_palette_mode(
        &g_world_shop_purchase_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    if (world_map_is_busy() != 0 || g_world_menu_option_count == 0 || g_world_shop_item_category == 4) {
        disabled = 1;
    }
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
}
