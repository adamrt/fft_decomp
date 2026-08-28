#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"

/* Handle item selection in the shop sell list.
 *
 * Mirrors world_shop_handle_item_purchase_menu with sell-side callbacks: half-price
 * values and the selected poached-item quantity. */
void world_shop_handle_item_sell_menu(void) {
    s16 x;
    s16 y;
    s32 input;
    s32 category;
    s32 item_list;
    s32 disabled;
    s8 result;

    if (g_world_shop_fur_purchase_list_initialized == 0) {
        if (world_thread_is_running(2) == 0) {
            world_thread_start(2, world_text_message_box_thread);
        }
        world_thread_set_parameters(2, 0x19, 0xF832, 0);
        g_world_shop_fur_purchase_list_initialized = 1;
        item_list = g_world_shop_id;
        if (g_world_shop_item_category != 4) {
            /* Forces the target's second category load; CSE otherwise reuses the first. */
            __asm__("" : : : "memory");
            category = g_world_shop_item_category + 1;
        } else {
            category = 7;
        }
        world_shop_build_item_list(-1, item_list, category, (world_item_list_entry_t*)g_world_menu_entry_ids, 0);
        world_gfx_get_sprite_slot(g_world_shop_item_category, &x, &y, g_world_menu_entry_ids);
        world_menu_init_scrollable_list(g_world_menu_entry_ids, x, y, g_world_text_item_names);
        g_world_menu_script_callbacks[0] = world_menu_get_entry_item_count_2;
        g_world_menu_script_callbacks[1] = world_menu_get_entry_item_count;
        g_world_menu_aux_callback = (s32 (*)(void))world_item_build_type_icon_source;
        g_world_menu_script_callbacks[3] = (s32 (*)(s32))world_menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4] = world_menu_get_item_half_price_display_value;
        g_world_menu_script_callbacks[5] = (s32 (*)(s32))world_item_count_selected_equipped_by_party;
        g_world_menu_script_callbacks[6] = (s32 (*)(s32))world_item_count_selected_owned_and_equipped;
        g_world_menu_script_callbacks[8] = (s32 (*)(s32))world_shop_get_selected_poached_item_quantity;
        g_world_shop_fur_equip_check_pending = 0;
    }
    world_script_set_vsync_mode_and_event_speed(2);
    if (g_world_shop_fur_equip_check_pending != 0) {
        world_thread_set_parameters(2, 0x19, -1, 0);
        result = world_thread_is_running(2);
        g_world_shop_fur_equip_check_pending = result;
        if (result == 0) {
            world_gfx_set_sprite_slot(g_world_shop_item_category, g_world_menu_cursor_position,
                g_world_menu_scroll_offset, (u16*)g_world_menu_entry_ids);
            g_world_shop_menu_step = 0x11;
            g_world_shop_fur_purchase_list_initialized = 0;
        }
        world_input_clear_state();
    } else {
        input = g_world_input_primary_repeat;
        if (input & PSX_PAD_CROSS) {
            g_world_shop_fur_purchase_list_initialized = 0;
            g_world_shop_menu_step = 0xE;
            world_thread_set_parameters(2, 0x19, 0xF830, 0);
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        } else if ((input & PSX_PAD_TRIANGLE) && g_world_menu_option_count != 0 && g_world_shop_item_category != 4) {
            g_world_shop_fur_equip_check_pending = 1;
        } else if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_world_menu_option_count == 0) {
                g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
            } else {
                g_world_shop_menu_step = 0x10;
                g_world_shop_quantity_menu_data.selection = 0;
                g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            }
        }
    }
    disabled = 0;
    world_menu_switch_item_category(g_world_input_secondary_repeat);
    world_menu_run_script_with_palette_mode(
        g_world_shop_fur_purchase_menu_script, g_world_input_primary_repeat, g_world_thread_task_active);
    if (world_map_is_busy() != 0 || g_world_menu_option_count == 0 || g_world_shop_item_category == 4) {
        disabled = 1;
    }
    world_menu_draw_pressable_button(3, 0xBA, 0x4C, (u16)g_world_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, 1);
    world_shop_run_obtain_gil_menu();
}
