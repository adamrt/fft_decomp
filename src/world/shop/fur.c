/* The Fur Shop sells poached stock and buys items through its own stock and price rules. */

#include "fft/game/item.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_list.h"
#include "fft/menu/menu_screen.h"
#include "fft/menu/menu_script.h"
#include "fft/menu/menu_selection.h"
#include "fft/menu/menu_window.h"
#include "fft/party/equipment.h"
#include "fft/party/formation.h"
#include "fft/party/formation_ability.h"
#include "fft/party/item_list.h"
#include "fft/party/shop.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/text/text.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Refusals of update_buy_quantity and
 * update_purchase_quantity; otherwise they return whether the
 * quantity prompt is still open. */
enum {
    SHOP_QUANTITY_HOLDING_MAX = -2, /* the party already holds 99 */
    SHOP_QUANTITY_CANNOT_AFFORD = -1
};
extern u8 g_shop_item_category;
extern menu_entry_t g_shop_quantity_menu_data[2];
extern u16 g_shop_sell_quantity_item; /* item being sold */

extern u8 g_shop_sell_total_shown; /* confirmation prompt shown */

extern menu_entry_t g_shop_fur_menu_thread_data;
extern u8 g_shop_fur_opened; /* fur shop opened */

static s8 s_fur_shop_access_granted;
extern u8 g_shop_fur_has_secret_hunt; /* any unit has Secret Hunt */

extern u8 g_shop_fur_has_poached_stock; /* any poached item in stock */

extern u8 g_shop_fur_waiting_message; /* waiting for message dismissal */

extern s8 g_shop_fur_purchase_list_initialized;
extern s8 g_shop_fur_equip_check_pending;
extern s16 g_shop_fur_purchase_message_wait;
extern u8 g_shop_fur_purchase_result; /* buy quantity menu thread (menu_managed_thread_start) */

extern u16 g_shop_fur_purchase_item; /* item being bought */

extern s32 g_shop_fur_purchase_price; /* half the item price */

extern u8 g_shop_fur_purchase_confirm_shown; /* confirmation prompt shown */

extern s8 g_shop_fur_sell_list_initialized;
extern s16 g_shop_fur_sale_message_wait;
extern u8 g_shop_fur_quantity_menu_running;
extern u8 g_shop_sell_confirm_script[235];
extern u8 g_shop_fur_purchase_menu_script[246];

/* The quantity menu passes this range to menu_screen_labeled_number_entry_thread. */
extern menu_screen_number_range_t g_shop_quantity_range;

static s32 update_sale_quantity(void);

static s32 update_purchase_quantity(void);

/* Fur Shop main menu step: without a unit that has Secret Hunt the shop turns the party away;
 * otherwise Buy (only when poached stock exists), Sell (only when the party has items) or Leave,
 * with a message explaining a refusal. */
void shop_fur_update_menu(void) {
    s32 i;

    if (g_shop_fur_opened == 0) {
        g_menu_list_selected_index = 0;
        menu_set_event_speed(0);
        g_shop_fur_has_secret_hunt = 0;
        g_shop_fur_has_poached_stock = 0;
        s_fur_shop_access_granted = 0;
        g_shop_fur_waiting_message = 0;
        for (i = 0; i < g_formation_unit_count; i++) {
            if (formation_ability_has_secret_hunt(i) != 0) {
                g_shop_fur_has_secret_hunt = 1;
            }
        }
        if (g_shop_fur_has_secret_hunt != 0) {
            for (i = 1; i < 0x100; i++) {
                if (shop_fur_adjust_stock(i, 0) != 0) {
                    g_shop_fur_has_poached_stock = 1;
                }
            }
        }
        if (g_shop_fur_has_secret_hunt == 0) {
            g_shop_menu_step = SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82D, 0);
            return;
        }
        s_fur_shop_access_granted = 1;
        g_shop_fur_opened = 1;
    }
    if (g_shop_fur_waiting_message != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF830, 0);
            g_shop_fur_waiting_message = 0;
        }
    } else if (menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, &g_shop_fur_menu_thread_data) == 0) {
        menu_selection_clear_records();
        if (g_menu_screen_selection_results[0] == 0) {
            if (g_shop_fur_has_poached_stock != 0) {
                g_shop_item_category = menu_selection_set_value(MENU_SCREEN_CURSOR_SHOP_ITEM_TAB, 0);
                g_shop_menu_step = SHOP_STEP_FUR_BUY_LIST;
                g_shop_fur_opened = 0;
            } else {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82E, 0);
                g_shop_fur_waiting_message = 1;
            }
        } else if (g_menu_screen_selection_results[0] == 1) {
            if (formation_check_inventory(0) != 0) {
                g_shop_menu_step = SHOP_STEP_FUR_SELL_LIST;
                g_shop_fur_opened = 0;
            } else {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF845, 0);
                g_shop_fur_waiting_message = 1;
            }
        } else {
            g_shop_menu_step = SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF831, 0);
            g_shop_fur_opened = 0;
        }
    }
    shop_draw_gil();
}

/* Query the selected poached-stock entry with a 0 quantity adjustment.
 *
 * An empty menu returns 0. */
static s32 get_poached_stock(s32 unused_row) {
    if (g_menu_list_entry_count != 0)
        return shop_fur_adjust_stock(g_item_list_entries[g_menu_list_selected_index], 0);
    return 0;
}

/* Fur shop: the list of the party's own inventory to sell; sibling of
 * shop_fur_buy_update_list.
 *
 * The callback block is written in ascending address order (794 -> 7ac), as
 * the sibling writes it. That makes gcc materialize the two twice-used
 * addresses (count_2 in a0, count in v1) up front at 0xb0-0xbc and defer
 * their stores; the target's emission order then follows. First-use order,
 * not store order, decides this.
 */
void shop_fur_sell_update_list(void) {
    s32 message;

    if (g_shop_fur_sell_list_initialized == 0) {
        message = formation_check_inventory(0) != 0 ? 0xf840 : 0xf845;

        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, message, 0);
        g_shop_item_category = menu_selection_set_value(MENU_SCREEN_CURSOR_SHOP_ITEM_TAB, 0);
        equipment_build_item_list(-1, g_shop_item_category + 1,
            g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_scrollable_list_init_core(g_item_list_entries, 0, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_row_get_equipped_count;
        g_menu_row_callbacks[1].value = equipment_row_get_item_count;
        g_menu_row_callbacks[2].icon = item_list_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_row_get_icon_source;
        g_menu_row_callbacks[4].value = shop_sell_get_price;
        g_menu_row_callbacks[5].value = equipment_row_get_equipped_count;
        g_menu_row_callbacks[6].value = equipment_row_get_item_count;
        g_shop_fur_sell_list_initialized = 1;
    }

    menu_set_event_speed(2);
    if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) != 0) {
        g_shop_fur_sell_list_initialized = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf830, 0);
        g_shop_menu_step = SHOP_STEP_FUR_MENU;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
    } else if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) != 0) {
        if (g_menu_list_entry_count == 0) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        } else {
            g_shop_menu_step = SHOP_STEP_FUR_SELL_CONFIRM;
            g_shop_quantity_menu_data[1].selected_index = 0;
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        }
    }
    shop_switch_item_tab(g_menu_input_newly_pressed);
    menu_script_run_with_palette_mode(g_shop_sell_confirm_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    shop_draw_gil();
}

/* Fur shop: confirm a sale. The party receives half price, and the sold copies join the
 * poached stock. */
void shop_fur_sell_update_confirm(void) {
    s32 result = 1;

    if (g_shop_fur_sale_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_shop_fur_sale_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF841, 0);
            g_shop_menu_step = SHOP_STEP_FUR_SELL_LIST;
            g_menu_screen_prompt_active = 0;
        }
    } else {
        result = update_sale_quantity();
    }
    if (result == 0) {
        if (g_menu_screen_selection_results[MENU_SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            shop_adjust_gil(shop_get_item_half_price(g_item_list_entries[g_menu_list_selected_index])
                * g_menu_screen_selection_results[MENU_SCREEN_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                -g_menu_screen_selection_results[MENU_SCREEN_SELECTION_SHOP_QUANTITY]);
            shop_fur_adjust_stock(g_item_list_entries[g_menu_list_selected_index],
                g_menu_screen_selection_results[MENU_SCREEN_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF837, 0);
            item_list_rebuild_category(g_shop_item_category);
            equipment_build_item_list(0, g_shop_item_category + 1,
                g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            menu_scrollable_list_open(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], (menu_instruction_t*)g_shop_sell_confirm_script);
            menu_window_set_scale_step(10);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_shop_fur_sale_message_wait = 1;
    }
    shop_switch_item_tab(0);
    menu_script_run_with_palette_mode(g_shop_sell_confirm_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    shop_draw_gil();
}

/* Fur shop: the quantity prompt of a sale, capped by the inventory count. */
static s32 update_sale_quantity(void) {
    s32 count;

    if (g_shop_fur_quantity_menu_running == 0) {
        g_shop_sell_total_shown = 0;
        g_shop_sell_quantity_item = g_item_list_entries[g_menu_list_selected_index];
        g_menu_screen_prompt_active = 1;
        count = item_adjust_inventory_count(g_shop_sell_quantity_item, 0);
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF842, 0);
            return -1;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF843, 0);
        g_shop_quantity_range.bounds.max = count;
        g_shop_sell_total_shown = 0;
        g_sound_suppressed = 1;
    }
    g_shop_fur_quantity_menu_running
        = menu_managed_thread_start(MENU_SCREEN_THREAD_SLOT_UPPER_MENU, g_shop_quantity_menu_data);
    if (thread_is_running(MENU_SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_shop_sell_total_shown == 0) {
        g_text_substitution_values[0] = shop_get_item_half_price(g_shop_sell_quantity_item)
            * g_menu_screen_selection_results[MENU_SCREEN_SELECTION_SHOP_QUANTITY];
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF844, 0);
        g_shop_sell_total_shown = 1;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (thread_is_running(MENU_SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_shop_sell_total_shown != 0) {
        g_shop_sell_total_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF843, 0);
    }
    if (g_shop_fur_quantity_menu_running == 0) {
        if (g_menu_screen_selection_results[MENU_SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = SOUND_SFX_SHOP_TRANSACTION;
        }
        if (g_menu_screen_selection_results[MENU_SCREEN_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_shop_fur_quantity_menu_running;
}

/* Price column of the Fur Shop buy list: half the item's price, flagged disabled when the entry is
 * unequippable. */
static s32 get_poached_price(s32 entry_index) {
    s32 display_value = shop_get_item_price(g_item_list_entries[entry_index]) >> 1;

    if ((g_item_list_entries[entry_index] & ITEM_ENTRY_UNEQUIPPABLE) != 0)
        display_value |= MENU_NUMBER_VALUE_DISABLED;
    return display_value;
}

/* Fur shop: the list of the shop's poached stock, bought at half price.
 *
 * Mirrors update_buy_list with the half-price value and
 * the poached-stock quantity as callbacks. */
void shop_fur_buy_update_list(void) {
    s16 selected_index;
    s16 scroll_base_index;
    s32 input;
    s32 disabled;
    s8 result;

    if (g_shop_fur_purchase_list_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF832, 0);
        g_shop_fur_purchase_list_initialized = 1;
        shop_build_stock_list(-1, g_menu_screen_context_id,
            g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_selection_restore_record(g_shop_item_category, &selected_index, &scroll_base_index, g_item_list_entries);
        menu_scrollable_list_init(
            g_item_list_entries, selected_index, scroll_base_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_row_get_equipped_count;
        g_menu_row_callbacks[1].value = equipment_row_get_item_count;
        g_menu_row_callbacks[2].icon = item_list_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_row_get_icon_source;
        g_menu_row_callbacks[4].value = get_poached_price;
        g_menu_row_callbacks[5].value = shop_buy_get_equipped_count;
        g_menu_row_callbacks[6].value = shop_buy_get_owned_count;
        g_menu_row_callbacks[8].value = get_poached_stock;
        g_shop_fur_equip_check_pending = 0;
    }
    menu_set_event_speed(2);
    if (g_shop_fur_equip_check_pending != 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        result = thread_is_running(THREAD_SCREEN_SLOT_MESSAGE);
        g_shop_fur_equip_check_pending = result;
        if (result == 0) {
            menu_selection_set_record(
                g_shop_item_category, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            g_shop_menu_step = SHOP_STEP_FUR_BUY_EQUIP;
            g_shop_fur_purchase_list_initialized = 0;
        }
        menu_input_clear_state();
    } else {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CROSS) {
            g_shop_fur_purchase_list_initialized = 0;
            g_shop_menu_step = SHOP_STEP_FUR_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF830, 0);
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        } else if ((input & PSX_PAD_TRIANGLE) && g_menu_list_entry_count != 0
            && g_shop_item_category != ITEM_MENU_TAB_OTHER) {
            g_shop_fur_equip_check_pending = 1;
        } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0) {
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
            } else {
                g_shop_menu_step = SHOP_STEP_FUR_BUY_CONFIRM;
                g_shop_quantity_menu_data[1].selected_index = 0;
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            }
        }
    }
    disabled = 0;
    shop_switch_item_tab(g_menu_input_secondary_repeat);
    menu_script_run_with_palette_mode(
        g_shop_fur_purchase_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    if (menu_screen_check_busy() != 0 || g_menu_list_entry_count == 0 || g_shop_item_category == ITEM_MENU_TAB_OTHER) {
        disabled = 1;
    }
    menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, SOUND_SFX_CONFIRM);
    shop_draw_gil();
}

/* Fur shop sibling of update_buy_confirm: the party pays half
 * price and the poached stock is reduced. */
void shop_fur_buy_update_confirm(void) {
    s32 result = 1;

    if (g_shop_fur_purchase_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_shop_fur_purchase_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF83B, 0);
            g_shop_menu_step = SHOP_STEP_FUR_BUY_LIST;
            g_menu_screen_prompt_active = 0;
        }
    } else {
        result = update_purchase_quantity();
    }
    if (result == 0) {
        if (g_menu_screen_selection_results[MENU_SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            shop_adjust_gil(-(shop_get_item_price(g_item_list_entries[g_menu_list_selected_index]) >> 1)
                * g_menu_screen_selection_results[MENU_SCREEN_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                g_menu_screen_selection_results[MENU_SCREEN_SELECTION_SHOP_QUANTITY]);
            shop_fur_adjust_stock(g_item_list_entries[g_menu_list_selected_index],
                -g_menu_screen_selection_results[MENU_SCREEN_SELECTION_SHOP_QUANTITY]);
            item_list_rebuild_category(g_shop_item_category);
            shop_build_stock_list(-1, g_menu_screen_context_id,
                g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            menu_scrollable_list_open(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], (menu_instruction_t*)g_shop_fur_purchase_menu_script);
            menu_window_set_scale_step(10);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF837, 0);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_shop_fur_purchase_message_wait = 1;
    }
    shop_switch_item_tab(0);
    menu_script_run_with_palette_mode(g_shop_fur_purchase_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C, 0, 1, SOUND_SFX_CONFIRM);
    shop_draw_gil();
}

/* Fur shop twin of update_buy_quantity: the quantity
 * prompt is capped by the 99-item inventory limit, the gil on hand at half
 * price and the poached stock. */
static s32 update_purchase_quantity(void) {
    s32 owned;
    s32 count;
    s32 price;
    s32 room;
    s32 stock;

    if (g_shop_fur_purchase_result == 0) {
        g_shop_fur_purchase_confirm_shown = 0;
        g_shop_fur_purchase_item = g_item_list_entries[g_menu_list_selected_index];
        g_menu_screen_prompt_active = 1;
        owned = item_get_total_count(g_shop_fur_purchase_item);
        if (owned >= 99) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF834, 0);
            return SHOP_QUANTITY_HOLDING_MAX;
        }
        count = shop_adjust_gil(0);
        price = shop_get_item_price(g_shop_fur_purchase_item) >> 1;
        count /= price;
        g_shop_fur_purchase_price = price;
        room = 99 - owned;
        if (room < count) {
            count = room;
        }
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF833, 0);
            return SHOP_QUANTITY_CANNOT_AFFORD;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF835, 0);
        stock = shop_fur_adjust_stock(g_shop_fur_purchase_item, 0);
        if (count >= stock) {
            count = stock;
        }
        g_shop_quantity_range.bounds.max = count;
        g_sound_suppressed = 1;
    }
    g_shop_fur_purchase_result
        = menu_managed_thread_start(MENU_SCREEN_THREAD_SLOT_UPPER_MENU, g_shop_quantity_menu_data);
    if (thread_is_running(MENU_SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_shop_fur_purchase_confirm_shown == 0) {
        g_text_substitution_values[0]
            = g_menu_screen_selection_results[MENU_SCREEN_SELECTION_SHOP_QUANTITY] * g_shop_fur_purchase_price;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF836, 0);
        g_shop_fur_purchase_confirm_shown = 1;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (thread_is_running(MENU_SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_shop_fur_purchase_confirm_shown != 0) {
        g_shop_fur_purchase_confirm_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF835, 0);
    }
    if (g_shop_fur_purchase_result == 0) {
        if (g_menu_screen_selection_results[MENU_SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = SOUND_SFX_SHOP_TRANSACTION;
        }
        if (g_menu_screen_selection_results[MENU_SCREEN_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_shop_fur_purchase_result;
}

/* Fur Shop equip-on-purchase step: run the shared equip-candidate step and return to the Fur Shop
 * buy list instead of the normal one. */
void shop_fur_buy_update_unit_preview(void) {
    shop_buy_update_unit_preview();
    if (g_shop_menu_step == SHOP_STEP_BUY_LIST)
        g_shop_menu_step = SHOP_STEP_FUR_BUY_LIST;
}
