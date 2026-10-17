/* The shop, including the main, gil, purchase, sell, fitting room, best fit preview and fur shop menus and steps. */

#include "fft/shop.h"
#include "fft/equipment.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/formation_item.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/message_box.h"
#include "fft/music.h"
#include "fft/name_entry.h"
#include "fft/party.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_panel.h"
#include "fft/unit_party.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static void soldier_office_draw_unit_count(void);
static void shop_fitting_undo(void);
static void shop_fitting_buy(void);
static void shop_fitting_take_off(s16 formation_index, s16 equipment_index);
static void shop_fitting_draw_cost(void);
static void shop_fitting_return_item(s16 unit_index, s16 equipment_index);
static s32 shop_fitting_checkout_prompt(void);
static s32 fur_shop_buy_quantity_prompt(void);
static s32 fur_shop_sell_quantity_prompt(void);
static s32 shop_buy_quantity_prompt(void);
static s32 shop_sell_quantity_prompt(void);
static void shop_draw_gil(void);
static void soldier_office_draw_fee(void);

/* Run a shop screen (item shop, Fur Shop or Soldier Office): set the primitive budgets,
 * load the town's backdrop and the shop music, then run frames through the step handlers
 * until the exit step, saving the party records and restoring VRAM on the way out. */
void shop_run_screen(s32 shop_type) {
    POLY_FT4 textured_quads[2 * 500];
    POLY_G4 gradient_quads[2 * 90];
    POLY_GT4 textured_gradient_quads[2 * 60];
    TILE tiles[2 * 30];
    LINE_F2 flat_lines[2 * 10];
    DR_MOVE draw_moves[2 * 30];
    DR_MODE draw_modes[2 * 30];
    DR_AREA draw_areas[2 * 10];
    u32 otags[2 * 64];
    menu_gfx_context_t buffers[2];
    u32* tim;
    RECT* tile_box = &g_shop_edge_tile_rect;
    s32 buttons;
    s32 ot_index;
    s32 disabled;
    s32 tim_index;
    s16 last_step;

    g_screen_ot_length = 0x40;
    g_screen_textured_quad_capacity = 0x1F4;
    g_gfx_poly_f3_capacity = 0;
    g_gfx_poly_ft3_capacity = 0;
    g_gfx_poly_f4_capacity = 0;
    g_gfx_poly_g3_capacity = 0;
    g_gfx_poly_gt3_capacity = 0;
    g_screen_gradient_quad_capacity = 0x5A;
    g_screen_textured_gradient_quad_capacity = 0x3C;
    g_screen_flat_line_capacity = 0xA;
    g_gfx_line_f3_capacity = 0;
    g_gfx_line_f4_capacity = 0;
    g_screen_gradient_line_capacity = 0;
    g_screen_line_g3_capacity = 0;
    g_gfx_line_g4_capacity = 0;
    g_gfx_tile_capacity = 0x1E;
    g_gfx_tile_1_capacity = 0;
    g_gfx_tile_8_capacity = 0;
    g_gfx_tile_16_capacity = 0;
    g_gfx_sprite_capacity = 0;
    g_gfx_sprite_8_capacity = 0;
    g_gfx_sprite_16_capacity = 0;
    g_screen_draw_move_capacity = 0x1E;
    g_screen_draw_area_capacity = 0xA;
    g_gfx_draw_mode_capacity = 0x1E;

    /* Until the loop, buttons holds the town background, ot_index the music and disabled the
     * shopkeeper's first line. */
    g_screen_context_id = shop_type;
    buttons = script_get_variable(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
    if (shop_type == SCREEN_CONTEXT_FUR_SHOP) {
        ot_index = MUSIC_FUR_SHOP;
        g_shop_menu_step = SHOP_STEP_FUR_MENU;
        disabled = 0xF82F;
        tim_index = 3;
    } else if (shop_type == SCREEN_CONTEXT_SOLDIER_OFFICE) {
        ot_index = MUSIC_SOLDIER_OFFICE;
        g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_MENU;
        disabled = 0xF820;
        tim_index = 6;
    } else {
        g_screen_context_id = script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
        ot_index = MUSIC_SHOP;
        disabled = 0xF801;
        tim_index = 0;
        g_shop_menu_step = SHOP_STEP_MAIN_MENU;
    }

    /* After the backdrop is in VRAM, the TIM buffer is reused for a VRAM backup that the exit loads back. */
    SetDispMask(0);
    while (DrawSync(1) != 0) { }
    gfx_move_image_and_wait(&g_shop_background_vram_rect, 0, 0);
    tim = shop_load_background(tim_index + buttons);
    gfx_load_image_and_wait(&g_shop_background_vram_rect, tim + 5);
    gfx_store_image_and_wait(&g_shop_vram_backup_rect, tim);

    screen_init_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, gradient_quads, textured_gradient_quads,
        flat_lines, 0, 0, 0, 0, 0, tiles, 0, 0, 0, 0, 0, 0, draw_moves, draw_areas, draw_modes);
    last_step = -1;
    formation_init_menu_state();
    g_shop_main_menu_thread_data.selected_index = 0;
    g_fur_shop_menu_thread_data.selected_index = 0;
    g_soldier_office_hire_menu_thread_data.selected_index = 0;

    screen_text_bind_section(SCREEN_TEXT_DATA_SHOP_DIALOGUE);
    g_text_section_pointers[SCREEN_TEXT_SECTION_SHOP_MENU_LABELS] = g_screen_text_shop_menu_labels_data;
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, disabled, 0);
    g_screen_music_slot = music_open_into_free_slot(ot_index);
    music_switch_track(g_screen_music_slot, 0x7F, 0);
    g_shop_background_visible = 1;
    g_formation_unit_banner_enabled = 0;

    while (g_shop_menu_step != SHOP_STEP_EXIT) {
        screen_present_frame(0, 0);
        menu_input_update_with_message_state();
        screen_input_update_lockout();
        screen_fade_update_in();
        if (g_menu_input_primary_repeat & PSX_PAD_SELECT) {
            screen_start_help_text(DIALOG_POSITION_CENTER);
        }
        if (g_screen_grid_id != MENU_GRID_HELP_NONE) {
            screen_update_grid_cursor(g_formation_unit_pointers[g_unit_selected_index]->uses_monster_skillset);
        }
        screen_set_palette_mode(g_menu_input_lock_mode);

        if (g_shop_background_visible != 0) {
            menu_gfx_enqueue_textured_quad(&g_shop_background_rect, 0, 0, 0, 0, 0x14C, 0, 1);
            gfx_enqueue_tile(&g_shop_backdrop_tile_rect, &g_screen_black_tile_color, 0, 0);
            gfx_enqueue_tile(tile_box, &g_screen_black_tile_color, 0, g_screen_ot_length - 1);
        }

        /* L1/R1 cycle units on the item and Fur Shop steps (those before the Soldier Office's). */
        if ((g_formation_unit_cycle_mode != 0 || g_formation_unit_browse_enabled != 0)
            && g_shop_menu_step < SHOP_STEP_SOLDIER_OFFICE_MENU) {
            disabled = 0;
            if (g_menu_input_lock_mode != 0 || g_screen_grid_id != MENU_GRID_HELP_NONE
                || g_formation_unit_cycle_mode == 1) {
                disabled = 1;
            }
            buttons = 0;
            if (disabled == 0) {
                buttons = g_menu_input_secondary_repeat;
            }
            if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0) {
                if (buttons & PSX_PAD_L1) {
                    g_unit_selected_index
                        = g_unit_selected_index == 0 ? g_formation_unit_count - 1 : g_unit_selected_index - 1;
                    buttons &= ~PSX_PAD_R1;
                } else if (buttons & PSX_PAD_R1) {
                    g_unit_selected_index
                        = g_unit_selected_index == g_formation_unit_count - 1 ? 0 : g_unit_selected_index + 1;
                }
            } else {
                buttons = 0;
            }
            menu_draw_pressable_button(
                MENU_PRESSABLE_BUTTON_L1, 0xC, 0xC, buttons & PSX_PAD_L1, disabled, SOUND_SFX_PAGE_SWITCH);
            menu_draw_pressable_button(
                MENU_PRESSABLE_BUTTON_R1, 0xE0, 0xC, buttons & PSX_PAD_R1, disabled, SOUND_SFX_PAGE_SWITCH);
        }

        g_shop_menu_step_handlers[g_shop_menu_step]();
        if (g_shop_menu_step != SHOP_STEP_RENAME_NAME_ENTRY && g_shop_menu_step != SHOP_STEP_HIRE_NAME_ENTRY
            && g_shop_menu_step != SHOP_STEP_FITTING_ROOM_REMOVE) {
            formation_restage_on_selection_change();
        }

        /* The event layer goes to OT 0x3C over the confirm prompts, the cost window and the Soldier Office. */
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 63;
        menu_draw_active_window_frames();
        if (g_shop_menu_step == SHOP_STEP_FUR_BUY_CONFIRM || g_shop_menu_step == SHOP_STEP_FUR_SELL_CONFIRM
            || g_shop_menu_step == SHOP_STEP_FITTING_ROOM_SELL || g_shop_menu_step == SHOP_STEP_BUY_CONFIRM
            || g_shop_menu_step == SHOP_STEP_SELL_CONFIRM || g_shop_fitting_cost_visible != 0
            || shop_type == SCREEN_CONTEXT_SOLDIER_OFFICE) {
            ot_index = 0x3C;
        } else {
            ot_index = 9;
        }
        menu_input_update_event_state(
            g_menu_gfx_context->otag + ot_index, screen_input_filter(), g_screen_draw_buffer_clip_y == 0 ? 0xF0 : 0);

        /* Entering a new step resets the per-step menu state. */
        if (g_shop_menu_step != last_step) {
            g_formation_unit_cycle_mode = 0;
            menu_set_vsync_mode_and_event_speed(0);
            g_help_message_text_id = 0;
            screen_reset_selection_results();
            last_step = g_shop_menu_step;
        }
        screen_sound_dispatch_effect();
    }

    music_unload_scenario_slot(g_screen_music_slot);
    formation_save_records();
    menu_set_vsync_mode_and_event_speed(0);
    screen_reset_context();
    gfx_load_image_and_wait(&g_shop_background_vram_rect, tim);
    heap_free(tim);
}

/* The on_change callback of the shop's quantity range (the range record whose max is
 * g_shop_quantity_limit): nothing to do. */
void shop_noop_callback(void) {
}

/* Shop main menu step: runs the Buy / Sell / Fitting Room menu and, when it
 * closes, moves to the chosen step (rows 0..2; the fitting room also sets its
 * message), or to the exit fade with its message for anything else. */
void shop_menu_step(void) {
    u8 running;

    if (g_shop_main_menu_running == 0) {
        g_formation_unit_cycle_mode = 0;
        g_menu_list_selected_index = 0;
    }
    running = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_shop_main_menu_thread_data);
    g_shop_main_menu_running = running;
    if (running == 0) {
        menu_clear_selection_records();
        if (g_screen_selection_results[0] == 0) {
            g_shop_menu_step = SHOP_STEP_BUY_LIST;
            g_shop_item_category = screen_init_cursor_position(SCREEN_CURSOR_SHOP_ITEM_TAB, 0);
        } else if (g_screen_selection_results[0] == 1) {
            g_shop_menu_step = SHOP_STEP_SELL_LIST;
        } else if (g_screen_selection_results[0] == 2) {
            g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
            g_unit_selected_index = 0;
            formation_stage_selected_unit();
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF818, 0);
        } else {
            g_shop_menu_step = SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF803, 0);
        }
    }
    shop_draw_gil();
}

/* Menu screen step: on the first frame reset the fade flag; once any of the
 * face buttons (0xf0) is pressed start the fade-out with a music fade, then
 * wait for the fade to complete before releasing the screen (g_shop_menu_step = -1). */
void shop_exit_step(void) {
    if (g_shop_exit_step_initialized == 0) {
        g_shop_exit_fade_started = 0;
        g_shop_exit_step_initialized = 1;
    }
    if (g_shop_exit_fade_started != 0) {
        if (screen_fade_update_out() == 0) {
            g_shop_menu_step = SHOP_STEP_EXIT;
            g_shop_exit_step_initialized = 0;
        }
    } else if (g_menu_input_newly_pressed & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
        screen_fade_start_increasing();
        music_set_target(0, 0xF0);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_shop_exit_fade_started = 1;
    }
    shop_draw_gil();
    if (g_screen_context_id == SCREEN_CONTEXT_SOLDIER_OFFICE) {
        soldier_office_draw_fee();
        soldier_office_draw_unit_count();
    }
}

/* Run the shop's war-funds menu script with shop_adjust_gil installed as the
 * script callback, then put the previous callback back. The step handlers call it. */
static void shop_draw_gil(void) {
    menu_callback_t* callback_slot;
    s32 (*previous_callback)(s32);

    callback_slot = g_menu_row_callbacks;
    previous_callback = callback_slot->value;
    callback_slot->value = shop_adjust_gil;
    menu_run_script_with_palette_mode(&g_shop_war_funds_script, 0, 0);
    callback_slot->value = previous_callback;
}

/* Returns the owned-and-equipped count for the selected item. */
static s32 shop_buy_get_owned_count(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count != 0) {
        result = item_get_total_count(g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}

/* Returns the party-equipped count for the selected item. */
static s32 shop_buy_get_equipped_count(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count != 0) {
        result = item_get_equipped_count(g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}

/* Full-price twin of fur_shop_buy_get_price: the menu
 * item's price with bit 30 copied from its 0x4000 entry flag. */
static s32 shop_buy_get_price(s32 entry_index) {
    s32 display_value = shop_get_item_price(g_item_list_entries[entry_index]);

    if ((g_item_list_entries[entry_index] & ITEM_ENTRY_UNEQUIPPABLE) != 0)
        display_value |= MENU_NUMBER_VALUE_DISABLED;
    return display_value;
}

/* Switch the active item category and restore its saved list cursor. */
static void shop_switch_item_tab(s32 input) {
    s16 x;
    s16 y;
    s32 category;
    s32 cursor_slot;
    s16 item;
    s32 description;

    category = menu_update_horizontal_selection_and_mark_change(
        5, SCREEN_CURSOR_SHOP_ITEM_TAB, input, SOUND_SFX_PAGE_SWITCH);
    cursor_slot = g_shop_item_category;
    if (cursor_slot != category) {
        if (g_shop_menu_step != SHOP_STEP_BUY_LIST && g_shop_menu_step != SHOP_STEP_FUR_BUY_LIST) {
            cursor_slot += 5;
        }
        menu_set_selection_record(
            cursor_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_shop_item_category = category;
        if (g_shop_menu_step == SHOP_STEP_BUY_LIST || g_shop_menu_step == SHOP_STEP_FUR_BUY_LIST) {
            shop_build_stock_list(-1, g_screen_context_id,
                category != ITEM_MENU_TAB_OTHER ? (category + 1) : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
                ITEM_LIST_MODE_ALL);
            cursor_slot = g_shop_item_category;
        } else {
            formation_item_build_list(-1, g_shop_item_category + 1,
                category != ITEM_MENU_TAB_OTHER ? category + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
                ITEM_LIST_MODE_ALL);
            cursor_slot = g_shop_item_category + 5;
        }
        menu_get_selection_record(cursor_slot, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
    }
    screen_draw_item_tabs(1, g_shop_item_category, g_menu_input_lock_mode);
    item = g_item_list_entries[g_menu_list_selected_index];
    if (item != -1) {
        description = (item & ITEM_ENTRY_ID_MASK) + HELP_TEXT_ID_ITEM_BASE;
    } else {
        description = 0;
    }
    g_help_message_text_id = description;
}

/* Shop buy list step: list the shop's items for the current tab; Cross returns to the main menu,
 * Triangle (except on the other-items tab, 4) opens the "who can equip" view of the highlighted
 * item once the message box has closed, and Circle starts buying it.
 */
void shop_buy_list_step(void) {
    s16 x;
    s16 y;
    s32 input;
    s32 disabled;
    s8 result;

    if (g_shop_buy_list_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF804, 0);
        g_shop_buy_list_initialized = 1;
        shop_build_stock_list(-1, g_screen_context_id,
            g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_get_selection_record(g_shop_item_category, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[1].value = equipment_get_row_item_count;
        g_menu_row_callbacks[2].icon = item_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
        g_menu_row_callbacks[4].value = shop_buy_get_price;
        g_menu_row_callbacks[5].value = shop_buy_get_equipped_count;
        g_menu_row_callbacks[6].value = shop_buy_get_owned_count;
        g_shop_buy_equip_check_pending = 0;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (g_shop_buy_equip_check_pending != 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        result = thread_is_running(THREAD_SCREEN_SLOT_MESSAGE);
        g_shop_buy_equip_check_pending = result;
        if (result == 0) {
            g_shop_buy_list_initialized = 0;
            menu_set_selection_record(
                g_shop_item_category, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            g_shop_menu_step = SHOP_STEP_BUY_EQUIP;
        }
        menu_input_clear_state();
    } else {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CROSS) {
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_shop_buy_list_initialized = 0;
            g_shop_menu_step = SHOP_STEP_MAIN_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF802, 0);
        } else if ((input & PSX_PAD_TRIANGLE) && g_menu_list_entry_count != 0
            && g_shop_item_category != ITEM_MENU_TAB_OTHER) {
            g_shop_buy_equip_check_pending = 1;
        } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0) {
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
            } else {
                g_shop_menu_step = SHOP_STEP_BUY_CONFIRM;
                g_shop_quantity_menu_data[1].selected_index = 0;
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            }
        }
    }
    disabled = 0;
    shop_switch_item_tab(g_menu_input_secondary_repeat);
    menu_run_script_with_palette_mode(&g_shop_buy_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    if (screen_is_busy() != 0 || g_menu_list_entry_count == 0 || g_shop_item_category == ITEM_MENU_TAB_OTHER) {
        disabled = 1;
    }
    menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, SOUND_SFX_CONFIRM);
    shop_draw_gil();
}

/*
 * Shop buy step: runs the purchase-quantity prompt for the highlighted item
 * and completes the purchase.
 *
 * On "yes" the gil is deducted (price times quantity), the items are added to
 * the inventory and message 0xF811 is shown; on "no" message 0xF80A. That
 * message, or the prompt's refusal (stack full or not affordable), stays
 * until any face button or d-pad press, which returns to the buy list. The
 * list, the Triangle button and the gil display are drawn every frame.
 */
void shop_buy_confirm_step(void) {
    s32 result = 1;

    if (g_shop_buy_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_shop_buy_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF805, 0);
            g_screen_prompt_active = 0;
            g_shop_menu_step = SHOP_STEP_BUY_LIST;
        }
    } else {
        result = shop_buy_quantity_prompt();
    }
    if (result == 0) {
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            shop_adjust_gil(-shop_get_item_price(g_item_list_entries[g_menu_list_selected_index])
                * g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF811, 0);
            item_rebuild_category_list(g_shop_item_category);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_shop_buy_message_wait = 1;
    }
    shop_switch_item_tab(0);
    menu_run_script_with_palette_mode(&g_shop_buy_menu_script, 0, g_menu_input_lock_mode);
    menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C, 0, 1, SOUND_SFX_CONFIRM);
    shop_draw_gil();
}

/* Runs the shop purchase-quantity menu for the highlighted item.
 *
 * On entry, caps the selectable quantity at min(gil / price, 99 - owned) and
 * returns -2 when the stack is already full or -1 when no unit is
 * affordable. Afterwards it returns the quantity menu thread state, updating
 * the total-price prompt while the confirmation sub-thread is running. */
static s32 shop_buy_quantity_prompt(void) {
    s32 owned;
    s32 count;
    s32 limit;

    if (g_shop_quantity_menu_running == 0) {
        g_shop_buy_total_shown = 0;
        g_screen_prompt_active = 1;
        g_shop_buy_item = g_item_list_entries[g_menu_list_selected_index];
        owned = item_get_total_count(g_shop_buy_item);
        if (owned >= 99) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF807, 0);
            return SHOP_QUANTITY_HOLDING_MAX;
        }
        count = shop_adjust_gil(0);
        g_shop_buy_unit_price = shop_get_item_price(g_shop_buy_item);
        count /= g_shop_buy_unit_price;
        limit = 99 - owned;
        if (limit < count) {
            count = limit;
        }
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF806, 0);
            return SHOP_QUANTITY_CANNOT_AFFORD;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF808, 0);
        g_shop_quantity_limit = count;
        g_shop_buy_total_shown = 0;
        g_sound_suppressed = 1;
    }
    g_shop_quantity_menu_running = menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, g_shop_quantity_menu_data);
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_shop_buy_total_shown == 0) {
        g_text_substitution_values[0]
            = g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY] * g_shop_buy_unit_price;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF809, 0);
        g_shop_buy_total_shown = 1;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_shop_buy_total_shown != 0) {
        g_shop_buy_total_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF808, 0);
    }
    if (g_shop_quantity_menu_running == 0) {
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = SOUND_SFX_SHOP_TRANSACTION;
        }
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_shop_quantity_menu_running;
}

/* Equip-menu marker for the selected item: 8 when the unit already wears it
 * or cannot equip it, -1 when it is a fresh equippable choice. */
static s32 shop_buy_get_equip_marker(s32 unit_index) {
    formation_unit_t* unit = g_formation_unit_pointers[unit_index];
    s32 i;

    for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
        if (unit->equipment[i] == g_shop_buy_equip_item) {
            return MENU_UNIT_READOUT_EQUIP_LABEL;
        }
    }
    if (equipment_can_equip_item(unit_index, g_shop_buy_equip_item) != 1) {
        return MENU_UNIT_READOUT_EQUIP_LABEL;
    }
    return -1;
}

/* Menu callback: sprite brightness for a unit on the shop's "who can equip"
 * view: 0x80 when the unit can equip the candidate item, 0x40 when not. */
static s32 shop_buy_get_equip_brightness(s32 unit_index) {
    return equipment_can_equip_item(unit_index, g_shop_buy_equip_item) == 1 ? 0x80 : 0x40;
}

/*
 * Run one frame of the shop's "who can equip" unit browser for the item
 * under the list cursor.
 *
 * Moving onto a unit that can equip the item stages it in the temporary
 * formation preview record and shows the stat preview; slots
 * already holding the item are flagged in g_formation_selected_unit_stat_detail.state.equipment. Cancel returns to the
 * buy list.
 */
void shop_buy_equip_step(void) {
    s32 unused[16];
    s32 input;
    u8 equipped;
    s32 i;
    s32 slot;
    s32 text_id;

    input = g_menu_input_secondary_repeat;
    if (g_shop_buy_equip_initialized == 0) {
        menu_set_vsync_mode_and_event_speed(0);
        g_formation_unit_browse_enabled = 0;
        g_shop_background_visible = 0;
        g_shop_buy_equip_item = g_item_list_entries[g_menu_list_selected_index];
        formation_init_grid_cursor(0);
        g_shop_buy_equip_preview_visible = 0;
        g_shop_buy_equip_initialized = 1;
    }
    g_shop_buy_equip_saved_browse = g_formation_unit_browse_enabled;
    formation_update_grid(0, g_formation_unit_browse_enabled == 0, 1, input, g_menu_input_lock_mode,
        shop_buy_get_equip_marker, shop_buy_get_equip_brightness);
    if (g_shop_buy_equip_saved_browse == 0) {
        if (g_formation_unit_browse_enabled != 0) {
            g_shop_buy_equip_last_unit = -1;
            return;
        }
    } else if (g_formation_unit_browse_enabled == 0) {
        g_unit_panel_status_display_thread_params.redraw_request = 1;
    }
    if (g_formation_unit_banner_enabled != 0 && g_formation_unit_browse_enabled == 0) {
        text_id = MENU_GRID_HELP_BANNER_TOP;
        if (g_formation_unit_banner_at_bottom != 0) {
            text_id = MENU_GRID_HELP_BANNER_BOTTOM;
        }
        g_help_message_text_id = text_id;
    } else {
        g_help_message_text_id = 0;
    }
    if (g_formation_unit_browse_enabled != 0) {
        if (g_unit_selected_index != g_shop_buy_equip_last_unit) {
            formation_stage_selected_unit();
            i = 0;
            equipped = 0;
            g_shop_buy_equip_last_unit = g_unit_selected_index;
            /* Highlight the slots that already hold the candidate. */
            for (; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                if (g_formation_unit_pointers[g_unit_selected_index]->equipment[i] == g_shop_buy_equip_item) {
                    equipped = 1;
                    g_formation_selected_unit_stat_detail.state.equipment[i] |= FORMATION_HIGHLIGHT;
                }
            }
            if (equipped != 0 || equipment_can_equip_item(g_unit_selected_index, g_shop_buy_equip_item) != 1) {
                g_shop_buy_equip_preview_visible = 0;
            } else {
                slot = formation_item_select_slot(FORMATION_PREVIEW_UNIT_INDEX, g_shop_buy_equip_item);
                formation_item_equip(FORMATION_PREVIEW_UNIT_INDEX, slot, g_shop_buy_equip_item);
                bcopy(g_formation_unit_pointers[FORMATION_PREVIEW_UNIT_INDEX]->equipment,
                    g_item_preview_stat_detail.state.equipment, sizeof(g_item_preview_stat_detail.state.equipment));
                equipment_calculate_total_swap_delta(&g_item_preview_stat_detail,
                    &g_unit_panel_selected_unit_stat_summary,
                    g_formation_unit_pointers[g_unit_selected_index]->equipment,
                    g_formation_unit_pointers[FORMATION_PREVIEW_UNIT_INDEX]->equipment);
                g_item_preview_stat_detail.state.two_hands = unit_is_two_handing_weapon(
                    g_item_preview_stat_detail.state.equipment, formation_ability_has_two_hands(g_unit_selected_index));
                g_shop_buy_equip_preview_visible = 1;
                g_item_preview_stat_detail.state.equipment[slot] = g_shop_buy_equip_item | SHOP_FITTING_ITEM;
            }
        }
    } else {
        g_shop_buy_equip_preview_visible = 0;
        if (g_shop_buy_equip_saved_browse == 0 && (input & PSX_PAD_CROSS)) {
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_formation_unit_banner_enabled = 0;
            g_shop_menu_step = SHOP_STEP_BUY_LIST;
            menu_set_vsync_mode_and_event_speed(2);
            g_shop_background_visible = 1;
            g_shop_buy_equip_initialized = 0;
        }
    }
    formation_toggle_preview_stats_window(g_shop_buy_equip_preview_visible);
    formation_toggle_stat_preview_panel(g_shop_buy_equip_preview_visible);
}

/* Menu callback: half price of the highlighted item, or 0 for an empty list.
 * The row argument is not read. */
static s32 shop_sell_get_price(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count == 0) {
        result = 0;
    } else {
        result = shop_get_item_half_price(g_item_list_entries[g_menu_list_selected_index]);
    }
    return result;
}

/* Shop sell list step: on first entry prompts 0xF80B (0xF80C when the
 * player already holds items), rebuilds the item list and installs the seven
 * column callbacks, then handles Cross (back to the main menu) and Circle
 * (sell the highlighted item).
 */
void shop_sell_list_step(void) {
    if (g_shop_sell_list_initialized == 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, formation_has_items(0) != 0 ? 0xF80C : 0xF80B, 0);
        g_shop_item_category = screen_init_cursor_position(SCREEN_CURSOR_SHOP_ITEM_TAB, 0);
        formation_item_build_list(-1, g_shop_item_category + 1,
            g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_init_scrollable_list_core(
            g_item_list_entries, g_menu_list_selected_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[1].value = equipment_get_row_item_count;
        g_menu_row_callbacks[2].icon = item_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
        g_menu_row_callbacks[4].value = shop_sell_get_price;
        g_menu_row_callbacks[5].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[6].value = equipment_get_row_item_count;
        g_shop_sell_list_initialized = 1;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_shop_sell_list_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF802, 0);
        g_shop_menu_step = SHOP_STEP_MAIN_MENU;
    } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_menu_list_entry_count == 0) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            g_shop_quantity_menu_data[1].selected_index = 0;
            g_shop_menu_step = SHOP_STEP_SELL_CONFIRM;
        }
    }
    shop_switch_item_tab(g_menu_input_secondary_repeat);
    menu_run_script_with_palette_mode(
        &g_shop_sell_list_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    shop_draw_gil();
}

/* Item sell confirmation step: pays half price times the quantity, removes the
 * items, prompts 0xF811 and rebuilds the list (0xF80A when nothing to sell).
 */
void shop_sell_confirm_step(void) {
    s32 result = 1;

    if (g_shop_sell_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_shop_sell_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80D, 0);
            g_shop_menu_step = SHOP_STEP_SELL_LIST;
            g_screen_prompt_active = 0;
        }
    } else {
        result = shop_sell_quantity_prompt();
    }
    if (result == 0) {
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            shop_adjust_gil(shop_get_item_half_price(g_item_list_entries[g_menu_list_selected_index])
                * g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                -g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF811, 0);
            item_rebuild_category_list(g_shop_item_category);
            formation_item_build_list(0, g_shop_item_category + 1,
                g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            screen_init_list(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_shop_sell_list_menu_script);
            menu_set_window_scale_step(10);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_shop_sell_message_wait = 1;
    }
    shop_switch_item_tab(0);
    menu_run_script_with_palette_mode(&g_shop_sell_list_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    shop_draw_gil();
}

/* Shop: the quantity prompt of a sale, capped by the inventory count.
 *
 * The first call returns -1 with message 0xF80E when none of the highlighted
 * item is in the inventory. Otherwise it returns the quantity menu's thread
 * state, showing the total (half price times quantity, message 0xF810) while
 * the confirmation child runs and the plain prompt (0xF80F) otherwise. The
 * final frame queues the transaction sound for "yes" and the confirm sound
 * for "no". */
static s32 shop_sell_quantity_prompt(void) {
    s32 count;

    if (g_shop_sell_result == 0) {
        g_shop_sell_confirm_shown = 0;
        g_shop_sell_item = g_item_list_entries[g_menu_list_selected_index];
        g_screen_prompt_active = 1;
        count = item_adjust_inventory_count(g_shop_sell_item, 0);
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80E, 0);
            return -1;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80F, 0);
        g_shop_quantity_limit = count;
        g_shop_sell_confirm_shown = 0;
        g_sound_suppressed = 1;
    }
    g_shop_sell_result = menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, g_shop_quantity_menu_data);
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_shop_sell_confirm_shown == 0) {
        g_text_substitution_values[0]
            = shop_get_item_half_price(g_shop_sell_item) * g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY];
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF810, 0);
        g_shop_sell_confirm_shown = 1;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_shop_sell_confirm_shown != 0) {
        g_shop_sell_confirm_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80F, 0);
    }
    if (g_shop_sell_result == 0) {
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = SOUND_SFX_SHOP_TRANSACTION;
        }
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_shop_sell_result;
}

/* Menu callback: add `delta` to the fitting room's running cost and return
 * the new total. */
static s32 shop_fitting_add_cost(s32 delta) {
    return g_shop_fitting_cost = delta + g_shop_fitting_cost;
}

/* Menu callback: no marker (-1) for any unit. */
static s32 shop_fitting_get_unit_marker(s32 unused_unit_index) {
    return -1;
}

/* Fitting-room menu step: on entry (after a button press) clear the trial equipment and its cost;
 * then either browse the formation (Circle or Triangle opens the fitting menu for the unit, Cross
 * goes to the checkout) or run the fitting menu thread and dispatch its selection to the next
 * shop step. */
void shop_fitting_menu_step(void) {
    s32 busy;

    if (g_shop_fitting_initialized == 0) {
        if (g_menu_input_newly_pressed == 0) {
            shop_draw_gil();
            return;
        }
        formation_init_grid_cursor(0);
        memset(g_shop_fitting_items, 0, sizeof(g_shop_fitting_items));
        /* Subtract the running cost from itself: back to 0. */
        shop_fitting_add_cost(-shop_fitting_add_cost(0));
        g_shop_fitting_unit_chosen = 0;
        g_shop_fitting_cost_visible = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_shop_fitting_initialized = 1;
        return;
    }
    if (g_shop_fitting_unit_chosen == 0) {
        g_shop_background_visible = 0;
        if (g_formation_unit_banner_enabled != 0) {
            g_help_message_text_id
                = (g_formation_unit_banner_at_bottom != 0) ? MENU_GRID_HELP_BANNER_BOTTOM : MENU_GRID_HELP_BANNER_TOP;
        } else {
            g_help_message_text_id = 0;
        }
        busy = screen_input_get_lockout();
        busy += formation_is_grid_scroll_needed();
        if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) && busy == 0) {
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_formation_unit_banner_enabled = 0;
            g_shop_menu_step = SHOP_STEP_FITTING_ROOM_CHECKOUT;
        } else if ((g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE)) && busy == 0) {
            g_formation_unit_cycle_mode = 2;
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            g_shop_fitting_unit_chosen = 1;
            g_formation_unit_browse_enabled = 1;
            g_formation_unit_banner_at_bottom = 0;
            g_shop_fitting_cost_visible = 1;
            g_shop_fitting_menu_thread_data[0].selected_index = 0;
            g_shop_fitting_menu_thread_data[1].selected_index = 0;
            menu_clear_selection_records();
        } else {
            formation_update_grid(
                1, 1, 0, g_menu_input_secondary_repeat, g_menu_input_lock_mode, shop_fitting_get_unit_marker, 0);
        }
    } else {
        g_shop_background_visible = 1;
        g_help_message_text_id = 0;
        if (menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_shop_fitting_menu_thread_data) == 0) {
            if (g_screen_selection_results[1] == 0) {
                g_shop_menu_step = SHOP_STEP_FITTING_ROOM_EQUIP;
            } else if (g_screen_selection_results[1] == 1) {
                g_shop_menu_step = SHOP_STEP_FITTING_ROOM_BEST_FIT;
            } else if (g_screen_selection_results[1] == 2) {
                g_shop_menu_step = SHOP_STEP_FITTING_ROOM_REMOVE;
            } else if (g_screen_selection_results[1] == 3) {
                if (g_screen_selection_results[2] == 0) {
                    g_shop_menu_step = SHOP_STEP_FITTING_ROOM_SELL_EQUIPPED;
                } else {
                    g_shop_menu_step = SHOP_STEP_FITTING_ROOM_SELL;
                }
            } else {
                g_shop_fitting_unit_chosen = 0;
                formation_init_selection_cursor(g_unit_selected_index);
                g_formation_unit_cycle_mode = 0;
                g_formation_unit_browse_enabled = 0;
                g_shop_fitting_cost_visible = 0;
                if (g_screen_selection_results[1] == 4) {
                    g_formation_unit_banner_enabled = 0;
                    g_shop_menu_step = SHOP_STEP_FITTING_ROOM_CHECKOUT;
                }
            }
            menu_init_selection_value(SCREEN_CURSOR_REMOVE_SLOT, 0);
            menu_init_selection_value(SCREEN_CURSOR_EQUIP_SLOT, 0);
        }
    }
    if (g_shop_fitting_cost_visible != 0) {
        shop_fitting_draw_cost();
    }
}

/* Shop fitting-room equipment step, the shop twin of formation_item_equip_menu.
 *
 * With the slot list active, steps the equipment-slot cursor and shows the
 * equipped item's description; confirming opens the shop item list for the
 * slot. In the item list, confirming an equippable item returns the unit's
 * current item to the fitting room, equips the shop item (bit 15 marks it as
 * unpaid), adds its price to the fitting-room cost and recalculates stats,
 * while moving the cursor previews the stat change. Cancel leaves the list,
 * or from the slot list goes back to the fitting room's unit grid. */
void shop_fitting_equip_step(void) {
    s16 x;
    s16 y;
    s32 prev;
    s32 slot;
    s32 value;
    u16 item;
    s16 equip;
    s32 result;
    s16* cursor_y;

    if (g_shop_fitting_equip_menu_initialized == 0) {
        g_shop_fitting_equip_menu_initialized = 1;
        g_shop_fitting_previewed_cursor = -1;
        g_shop_fitting_item_list_open = 0;
        g_menu_list_selected_index = 0;
    }

    /* Cross closes the open list below, or leaves for the fitting room's unit grid. */
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        if (g_shop_fitting_item_list_open != 0) {
            g_shop_fitting_item_list_close = 1;
        } else {
            g_shop_fitting_equip_menu_initialized = 0;
            g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
        }
    } else if (g_shop_fitting_item_list_open == 0) {
        prev = g_shop_fitting_equip_slot;
        g_shop_fitting_equip_slot = menu_update_vertical_selection_and_mark_change(
            MENU_EQUIPMENT_SLOT_COUNT, SCREEN_CURSOR_EQUIP_SLOT, g_menu_input_primary_repeat, SOUND_SFX_PAGE_SWITCH);
        if (prev != g_shop_fitting_equip_slot) {
            g_menu_list_selected_index = 0;
        }

        if (g_formation_unit_pointers[g_unit_selected_index]->equipment[g_shop_fitting_equip_slot] != ITEM_ID_NOTHING) {
            value = (g_formation_unit_pointers[g_unit_selected_index]->equipment[g_shop_fitting_equip_slot]
                        & ITEM_ENTRY_ID_MASK)
                + HELP_TEXT_ID_ITEM_BASE;
        } else {
            value = -1;
        }
        g_help_message_text_id = value;

        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            g_formation_unit_cycle_mode = 1;
            g_shop_fitting_item_list_open = 1;
            g_shop_fitting_previewed_cursor = -1;
            g_shop_fitting_cost_visible = 0;
            g_formation_change_banner_thread_params.style = 1;
            shop_build_stock_list(g_unit_selected_index, g_screen_context_id, g_shop_fitting_equip_slot,
                g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);

            /* Both hand slots share selection record 0, so the record is the slot less one. */
            slot = 0;
            if (g_shop_fitting_equip_slot != 0) {
                slot = g_shop_fitting_equip_slot - 1;
            }
            menu_get_selection_record(slot, &x, &y, g_item_list_entries);
            menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
            g_menu_row_callbacks[1].value = equipment_get_row_item_count;
            g_menu_row_callbacks[2].icon = item_build_type_icon_source;
            g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
            g_menu_row_callbacks[4].value = shop_buy_get_price;
            menu_set_vsync_mode_and_event_speed(2);
        }
    } else {
        item = g_item_list_entries[g_menu_list_selected_index];

        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0 || (item & ITEM_ENTRY_UNEQUIPPABLE)) {
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
            } else {
                equip = item;
                result = formation_item_can_equip_in_slot(g_unit_selected_index, g_shop_fitting_equip_slot, equip);
                if (result < 0) {
                    g_sound_queued_effect_id = SOUND_SFX_INVALID;
                } else {
                    shop_fitting_return_item(g_unit_selected_index, g_shop_fitting_equip_slot);
                    g_formation_unit_pointers[g_unit_selected_index]->equipment[g_shop_fitting_equip_slot]
                        = item | SHOP_FITTING_ITEM;
                    shop_fitting_add_cost(shop_get_item_price(equip));
                    /* 0: the item clashes with the other hand, which goes back to the fitting room too. */
                    if (result != 1) {
                        shop_fitting_return_item(g_unit_selected_index, g_shop_fitting_equip_slot == 0);
                    }
                    g_shop_fitting_item_list_close = 1;
                    g_sound_queued_effect_id = SOUND_SFX_EQUIP;
                    formation_recalculate_selected_stats();
                }
            }
        }

        /* Try the item on the preview unit, take the stat delta, then restore the preview unit's equipment. */
        if (g_menu_list_entry_count != 0 && g_shop_fitting_previewed_cursor != g_menu_list_selected_index) {
            g_shop_fitting_previewed_cursor = g_menu_list_selected_index;
            result = formation_item_equip(FORMATION_PREVIEW_UNIT_INDEX, g_shop_fitting_equip_slot, item);
            if (result == 1) {
                formation_toggle_preview_stats_window(1);
                g_unit_panel_preview_stats_thread_params.redraw_request = result;
                equipment_calculate_total_swap_delta(&g_item_preview_stat_detail,
                    &g_unit_panel_selected_unit_stat_summary,
                    g_formation_unit_pointers[g_unit_selected_index]->equipment, g_formation_temp_unit->equipment);
                bcopy(g_formation_unit_pointers[g_unit_selected_index]->equipment, g_formation_temp_unit->equipment,
                    sizeof(g_formation_temp_unit->equipment));
            } else {
                formation_toggle_preview_stats_window(0);
            }
            g_unit_panel_status_display_thread_params.redraw_request = 1;
        }

        menu_run_script_with_palette_mode(
            &g_shop_fitting_item_list_menu_script, g_menu_input_primary_repeat, screen_is_busy());
        g_help_message_text_id = g_menu_list_entry_count != 0
            ? (g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_ID_MASK) + HELP_TEXT_ID_ITEM_BASE
            : -1;
    }

    if (g_shop_fitting_item_list_close != 0) {
        g_shop_fitting_cost_visible = 1;
        g_shop_fitting_item_list_close = 0;
        g_formation_change_banner_thread_params.style = 0;
        g_shop_fitting_item_list_open = 0;
        g_formation_unit_cycle_mode = 2;
        menu_set_vsync_mode_and_event_speed(0);
        formation_toggle_preview_stats_window(0);
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        slot = 0;
        if (g_shop_fitting_equip_slot != 0) {
            slot = g_shop_fitting_equip_slot - 1;
        }
        menu_set_selection_record(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
    }

    cursor_y = &g_shop_fitting_slot_cursor_point.y;
    *cursor_y = g_shop_fitting_equip_slot * 16 + 0x90;
    gfx_set_otag_index(0x3d);
    screen_draw_sliding_cursor(&g_shop_fitting_slot_cursor_point, &g_shop_fitting_slot_cursor_anim,
        g_shop_fitting_item_list_open != 0 || g_menu_input_lock_mode != 0);
    if (g_shop_fitting_cost_visible != 0) {
        shop_fitting_draw_cost();
    }
}

/* Row callback: the war funds left after paying for the items being tried on; the row is ignored. */
static s32 shop_fitting_get_gil_left(s32 unused_row) {
    s32 gil = shop_adjust_gil(0);

    return gil - shop_fitting_add_cost(0);
}

/* Report whether menu input is locked or the screen menu slot's thread still runs.
 *
 * Its only caller passes 0, but the body never reads that argument: `$a0` is
 * overwritten with 6 before any use and the result starts from zero. */
static s32 shop_is_menu_busy(s32 unused) {
    s32 result = 0;

    if (g_menu_input_lock_mode != 0) {
        result = 1;
    } else if (thread_is_running(THREAD_SCREEN_SLOT_MENU) != 0) {
        result = 1;
    }
    return result;
}

/* Run the fitting room's cost display script with its three menu value
 * callbacks installed (gil, fitting cost, gil minus cost); the script gets
 * no input while a menu is busy. */
static void shop_fitting_draw_cost(void) {
    g_menu_row_callbacks[0].value = shop_adjust_gil;
    g_menu_row_callbacks[1].value = shop_fitting_add_cost;
    g_menu_row_callbacks[2].value = shop_fitting_get_gil_left;
    menu_run_script_with_palette_mode(
        &g_shop_fitting_cost_menu_script, g_menu_input_primary_repeat, shop_is_menu_busy(0));
}

/* Fitting-room counterpart of formation_item_remove_menu: takes one equipment
 * slot off the selected unit, refunding a shop item's price from the fitting
 * room cost, and previews the stat change of the slot under the cursor
 * against the unit's own item held in the fitting room.
 *
 * The prologue's $a0 is not an argument register here. The callee,
 * formation_restage_on_selection_change, takes no arguments; $a0 simply holds the copy of the selected-unit index that
 * the store to g_shop_fitting_remove_previewed_unit_index consumes, while the compare keeps the load in $v1. A
 * one-argument prototype would pull the load pseudo into $a0 and push the copy into $v1; pins and launders cannot
 * compensate, because the target's copy sits in the beq's delay slot and a tied asm is never a delay-slot candidate. */
void shop_fitting_remove_step(void) {
    /* The target reserves 64 local bytes whose contents are not accessed;
     * saved registers sit at 0x60..0x70 under a -0x78 frame. */
    s32 unused[16];
    s8 slot;
    s32 index;
    s32 value;
    s32 equipped_item;
    s16* cursor_y;
    s16 unit;

    if (g_shop_fitting_remove_previewed_unit_index != g_unit_selected_index) {
        unit = g_unit_selected_index;
        g_shop_fitting_remove_previewed_unit_index = unit;
        g_shop_fitting_remove_preview_initialized = 0;
        formation_restage_on_selection_change();
        menu_input_clear_state();
    }
    if (g_shop_fitting_remove_preview_initialized == 0) {
        g_shop_fitting_remove_previewed_equip_slot = -1;
        g_shop_fitting_remove_preview_initialized = 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        formation_toggle_preview_stats_window(0);
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
        g_shop_fitting_remove_preview_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
    }
    slot = menu_update_vertical_selection_and_mark_change(
        MENU_EQUIPMENT_SLOT_COUNT, SCREEN_CURSOR_REMOVE_SLOT, g_menu_input_primary_repeat, SOUND_SFX_PAGE_SWITCH);
    if (g_formation_unit_pointers[g_unit_selected_index]->equipment[(u8)slot] != ITEM_ID_NOTHING) {
        value = (g_formation_unit_pointers[g_unit_selected_index]->equipment[(u8)slot] & ITEM_ENTRY_ID_MASK)
            + HELP_TEXT_ID_ITEM_BASE;
    } else {
        value = -1;
    }
    g_help_message_text_id = value;
    cursor_y = &g_shop_fitting_remove_cursor_point.y;
    *cursor_y = (u8)slot * 16 + 0x90;
    gfx_set_otag_index(0x3D);
    screen_draw_sliding_cursor(
        &g_shop_fitting_remove_cursor_point, &g_shop_fitting_remove_cursor_anim, menu_input_get_lock_mode(0));
    /* Only a tried-on shop item (bit 15) can be taken off here; its price comes off the cost. */
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        equipped_item = g_formation_unit_pointers[g_unit_selected_index]->equipment[(u8)slot];
        if ((equipped_item >> 15) == 0) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        } else {
            shop_fitting_add_cost(-shop_get_item_price(equipped_item & ITEM_ENTRY_ID_MASK));
            shop_fitting_take_off(g_unit_selected_index, slot & 0xFF);
            formation_recalculate_selected_stats();
            g_sound_queued_effect_id = SOUND_SFX_UNEQUIP;
            g_shop_fitting_remove_previewed_equip_slot = -1;
        }
    }
    index = (u8)slot;
    if (g_shop_fitting_remove_previewed_equip_slot != index) {
        equipped_item = g_formation_unit_pointers[g_unit_selected_index]->equipment[index];
        g_shop_fitting_remove_previewed_equip_slot = slot;
        if ((equipped_item >> 15) != 0) {
            formation_toggle_preview_stats_window(1);
            equipment_calculate_swap_stat_delta(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
                equipped_item, g_shop_fitting_items[g_unit_selected_index][index], index);
            g_unit_panel_preview_stats_thread_params.redraw_request = 1;
        } else {
            formation_toggle_preview_stats_window(0);
        }
        g_unit_panel_status_display_thread_params.redraw_request = 1;
    }
    if (g_shop_fitting_cost_visible != 0) {
        shop_fitting_draw_cost();
    }
}

/*
 * Fitting room step that prices the tried-on equipment.
 *
 * Runs the checkout prompt: done returns to the shop's main menu (message
 * 0xf802); canceled shows message 0xf818 until any face button, which
 * returns to the fitting room with the unit's stats recalculated.
 */
void shop_fitting_checkout_step(void) {
    s32 result;

    g_shop_background_visible = 1;
    if (g_shop_fitting_checkout_complete != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_shop_fitting_checkout_complete = 0;
            g_formation_unit_banner_enabled = 1;
            formation_recalculate_selected_stats();
        }
    } else {
        result = shop_fitting_checkout_prompt();
        if (result == SHOP_FITTING_CHECKOUT_DONE) {
            g_shop_menu_step = SHOP_STEP_MAIN_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf802, 0);
            g_shop_fitting_initialized = 0;
        } else if (result == SHOP_FITTING_CHECKOUT_CANCELED) {
            g_shop_fitting_checkout_complete = 1;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf818, 0);
        }
    }
    shop_draw_gil();
}

/*
 * Run one frame of the fitting-room checkout prompt.
 *
 * Opens the pay prompt when the party can afford the fitting-room cost and
 * the "not enough gil" prompt otherwise; an empty cart is bought
 * immediately. Returns 1 while a prompt is open; 0 and -1 are the two
 * closing outcomes shop_fitting_checkout_step dispatches on.
 */
static s32 shop_fitting_checkout_prompt(void) {
    s32 balance;
    s32 cost;

    if (g_shop_fitting_checkout_prompt_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
            return SHOP_FITTING_CHECKOUT_PROMPT_OPEN;
        }
        g_shop_fitting_checkout_confirm_menu.selected_index = 0;
        g_shop_fitting_checkout_short_gil_menu.selected_index = 0;
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        balance = shop_adjust_gil(0) - (cost = shop_fitting_add_cost(0));
        if (cost == 0) {
            shop_fitting_buy();
            return SHOP_FITTING_CHECKOUT_DONE;
        }
        if (balance >= 0) {
            g_sound_suppressed = 1;
            g_shop_fitting_checkout_prompt_menu = &g_shop_fitting_checkout_confirm_menu;
            g_text_substitution_values[0] = cost;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf819, 0);
            g_shop_fitting_checkout_insufficient_gil = 0;
        } else {
            g_shop_fitting_checkout_prompt_menu = &g_shop_fitting_checkout_short_gil_menu;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf81a, 0);
            g_shop_fitting_checkout_insufficient_gil = 1;
        }
        g_shop_fitting_checkout_prompt_initialized = 1;
        g_shop_fitting_checkout_prompt_active = 1;
    }
    if (g_shop_fitting_checkout_prompt_active != 0) {
        g_shop_fitting_checkout_prompt_active
            = menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, g_shop_fitting_checkout_prompt_menu);
        if (g_shop_fitting_checkout_prompt_active != 0) {
            return SHOP_FITTING_CHECKOUT_PROMPT_OPEN;
        }
        g_shop_fitting_checkout_prompt_result = g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT];
        if (g_shop_fitting_checkout_insufficient_gil == 0) {
            g_sound_suppressed = 0;
            if (g_shop_fitting_checkout_prompt_result == 0) {
                balance = -shop_fitting_add_cost(0);
                shop_adjust_gil(balance);
                shop_fitting_add_cost(balance);
                shop_fitting_buy();
                formation_save_records();
                g_sound_queued_effect_id = SOUND_SFX_SHOP_TRANSACTION;
                item_rebuild_category_list(0);
                item_rebuild_category_list(1);
                item_rebuild_category_list(2);
                item_rebuild_category_list(3);
                item_rebuild_category_list(4);
                g_shop_fitting_checkout_prompt_initialized = 0;
                return SHOP_FITTING_CHECKOUT_DONE;
            }
            if (g_shop_fitting_checkout_prompt_result == 1) {
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            }
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf81c, 0);
            return SHOP_FITTING_CHECKOUT_PROMPT_OPEN;
        }
        if (g_shop_fitting_checkout_prompt_result != 1) {
            shop_fitting_undo();
            g_shop_fitting_checkout_prompt_initialized = 0;
            return SHOP_FITTING_CHECKOUT_DONE;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_shop_fitting_checkout_prompt_initialized = 0;
        return SHOP_FITTING_CHECKOUT_CANCELED;
    }
    if (menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, &g_shop_fitting_checkout_short_gil_menu) != 0) {
        return SHOP_FITTING_CHECKOUT_PROMPT_OPEN;
    }
    g_shop_fitting_checkout_prompt_initialized = 0;
    if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] != 1) {
        if (g_shop_fitting_checkout_prompt_result != 0) {
            shop_fitting_undo();
            return SHOP_FITTING_CHECKOUT_DONE;
        }
        return SHOP_FITTING_CHECKOUT_DONE;
    }
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
    return SHOP_FITTING_CHECKOUT_CANCELED;
}

/* Undo the fitting room: give every unit back the items it set aside there and clear the
 * reservations. A slot holding a tried-on shop item (bit 14 or 15 set) is restored even when
 * nothing was set aside, which empties it. */
static void shop_fitting_undo(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_formation_unit_count; i++) {
        for (j = 0; j < MENU_EQUIPMENT_SLOT_COUNT; j++) {
            if ((g_formation_unit_pointers[i]->equipment[j] >> 14) != 0
                || g_shop_fitting_items[i][j] != ITEM_ID_NOTHING) {
                g_formation_unit_pointers[i]->equipment[j] = g_shop_fitting_items[i][j];
            }
            g_shop_fitting_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}

/*
 * Shop "best fit / fitting room" preview step, entry 8
 * (SHOP_STEP_FITTING_ROOM_BEST_FIT) of g_shop_menu_step_handlers.
 *
 * On entry it stages the selected unit's four weapon/armor slots, fills any
 * slot already taken from the shop with that slot's fitting-room reservation,
 * asks shop_fitting_build_best_fit for a suggested set,
 * restores the staged slots and previews
 * the resulting stat delta. It then runs the confirmation menu; on confirm it
 * commits the suggested set slot by slot, returning each displaced item to the
 * fitting room or refunding a shop item's price, and returns to the fitting
 * room's unit grid.
 *
 * The gate g_shop_fitting_best_fit_menu_open is `s8`, not `u8` (the target
 * sign-loads it, `lb`).
 * Loop 2 computes both the suggested item and the unit's own slot at the
 * dominator; deferring either load into an arm costs a duplicate load and the
 * hoist of the selected-unit index.
 *
 * `item` is s32. As `s16` it is an HImode pseudo, so gcc emits a truncation
 * copy (`move s3,s0`) and stores through the copy while testing and passing
 * the original: seven callee-saved registers where the target uses six
 * (s0-s4 + ra), an extra save and a 136-byte frame. With `item` wide,
 * `shifted = suggested << 16; item = shifted >> 16;` restores the target's
 * `lhu` / `sll` / `sra` triple and lets the shifted value serve the sign test
 * (`bgez`); an HImode value folds the pair into one `lh`.
 *
 * The frame pad is split around the staging buffer -- `unused[16]`, `staged`,
 * `unused_tail[2]` -- to keep the buffer at the target's sp+80
 * (`.frame vars=88, regs=6/0, args=16`); a single `unused[18]` pushes it to
 * sp+88.
 *
 * The target has `move s1,v0` after menu_start_managed_thread and no
 * initialization of s1 at loop 2's head: loop 2's counter is the run_thread
 * result's allocno, so the source assigns the result to `i` and the loop is
 * written `for (; i < MENU_EQUIPMENT_SLOT_ACCESSORY; i++)`. A fresh block-local takes a call-clobbered
 * register; reusing a function-scope allocno that is already dead puts the
 * counter in the target's callee-saved s1 and decides which of the two loop
 * quantities is allocated first.
 *
 * In the loop-1 preheader the target computes
 * `&g_formation_unit_pointers[idx]` first (into a1) and the
 * fitting-room row base second (into a0). Giving the unit slot its own local
 * (`unit_slot`), assigned just before `reserved`, and reading the loop body
 * through `(*unit_slot)->equipment[i]`, puts the two preheader address
 * computations in that order. The `lw` stays inside the loop, since the store
 * through `reserved` may alias the pointer array.
 *
 * Within `reserved`'s address expression the target materializes the
 * fitting-room base `lui`/`addiu` before the `idx*5*2` index arithmetic. cc1
 * itself emits `addu`/`sll`/`la`/`addu` in the other order under one `.loc`,
 * because the symbol canonicalizes to the second operand of the address PLUS
 * and is expanded last; no operand respelling changes that (`fold` rewrites
 * `base - -(idx * 5)` straight back to a PLUS). Giving the row base its own
 * local and its own statement (`room_base = &g_shop_fitting_items[0][0];`
 * then `reserved = room_base + idx * 5;`) forces the `la` to be emitted at
 * that statement, ahead of the index arithmetic.
 *
 * The confirmation result g_screen_selection_results[5] is read directly
 * each time. Its address stays in one register across the calls and the last
 * read is scheduled ahead of the two byte stores that precede it in source.
 */
void shop_fitting_best_fit_step(void) {
    /* The target reserves vars the code never touches: args 16 + vars 88 +
     * 6 saves 24 = the frame of 128, while only the 8-byte staging buffer at
     * sp+80 is accessed. */
    s32 unused[16];
    u16 staged[4];
    s32 unused_tail[2];
    s32 i;
    s32 item;

    if (g_shop_fitting_best_fit_menu_open == 0) {
        formation_unit_t** unit_slot;
        s16* room_base;
        s16* reserved;

        g_shop_fitting_best_fit_menu.selected_index = 0;
        bcopy(g_formation_unit_pointers[g_unit_selected_index]->equipment, staged, sizeof(staged));
        unit_slot = &g_formation_unit_pointers[g_unit_selected_index];
        room_base = &g_shop_fitting_items[0][0];
        reserved = room_base + g_unit_selected_index * MENU_EQUIPMENT_SLOT_COUNT;
        for (i = 0; i < MENU_EQUIPMENT_SLOT_ACCESSORY; i++, reserved++) {
            if (((*unit_slot)->equipment[i] >> 15) != 0) {
                (*unit_slot)->equipment[i] = *reserved;
            }
        }
        shop_fitting_build_best_fit(
            g_unit_selected_index, g_screen_context_id, g_item_preview_stat_detail.state.equipment);
        g_item_preview_stat_detail.state.equipment[MENU_EQUIPMENT_SLOT_ACCESSORY]
            = g_formation_unit_pointers[g_unit_selected_index]->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY];
        bcopy(staged, g_formation_unit_pointers[g_unit_selected_index]->equipment, sizeof(staged));
        equipment_calculate_total_swap_delta(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
            g_formation_unit_pointers[g_unit_selected_index]->equipment,
            (u16*)g_item_preview_stat_detail.state.equipment);
        g_item_preview_stat_detail.state.two_hands = unit_is_two_handing_weapon(
            g_item_preview_stat_detail.state.equipment, formation_ability_has_two_hands(g_unit_selected_index));
        formation_toggle_stat_preview_panel(1);
        formation_toggle_preview_stats_window(1);
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
        g_shop_fitting_best_fit_menu_open = 1;
        g_formation_unit_cycle_mode = 1;
        g_sound_suppressed = 1;
    }
    i = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_shop_fitting_best_fit_menu);
    if (i == 0) {
        if (g_screen_selection_results[5] == 0) {
            g_sound_queued_effect_id = SOUND_SFX_EQUIP;
        }
        if (g_screen_selection_results[5] == 1) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        }
        formation_toggle_stat_preview_panel(0);
        formation_toggle_preview_stats_window(0);
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        g_shop_fitting_best_fit_menu_open = 0;
        g_formation_unit_cycle_mode = 2;
        if (g_screen_selection_results[5] == 0) {
            for (; i < MENU_EQUIPMENT_SLOT_ACCESSORY; i++) {
                u16 suggested;
                s32 shifted;
                s32 current;

                /* The target reads the suggested item unsigned and widens it
                 * explicitly, reusing the shifted value for the sign test
                 * (`lhu` / `sll 16` / `sra 16`, then `bgez` on the shift), and
                 * it loads the unit's own slot ONCE at the loop head -- the
                 * `sra ,0xf` lands in the `bgez` delay slot and the same
                 * register is sign-extended again for shop_get_item_price. */
                suggested = g_item_preview_stat_detail.state.equipment[i];
                shifted = suggested << 16;
                item = shifted >> 16;
                current = g_formation_unit_pointers[g_unit_selected_index]->equipment[i];
                if (item == 0 || shifted < 0) {
                    shop_fitting_return_item(g_unit_selected_index, i);
                    shop_fitting_add_cost(shop_get_item_price(item));
                } else if ((current >> 15) != 0) {
                    shop_fitting_add_cost(-shop_get_item_price(current));
                    g_shop_fitting_items[g_unit_selected_index][i] = ITEM_ID_NOTHING;
                }
                g_formation_unit_pointers[g_unit_selected_index]->equipment[i] = item;
            }
            formation_recalculate_selected_stats();
        }
        g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
        g_sound_suppressed = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
    }
    if (g_shop_fitting_cost_visible != 0) {
        shop_fitting_draw_cost();
    }
}

/*
 * Run one frame of the shop's sell-equipped-item menu.
 *
 * The cursor walks the selected unit's five equipment slots; confirming a
 * slot the unit owns (non-empty, not taken from the shop) opens a sale prompt
 * for half the item's price and, on acceptance, credits the gil and clears
 * the slot. Cross returns to the fitting room's unit grid.
 */
void shop_fitting_sell_equipped_step(void) {
    s32 close = 0;
    u32 item;
    s32 text_id;
    u32 input;

    if (g_shop_fitting_sell_equipped_menu_open == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80c, 0);
            g_shop_fitting_sell_equipped_prompt_active = 0;
            g_formation_unit_cycle_mode = 1;
            g_shop_fitting_sell_equipped_menu_open = 1;
            g_shop_fitting_sell_equipped_result_shown = 0;
        }
    } else {
        if (g_shop_fitting_sell_equipped_result_shown != 0) {
            if (g_menu_input_primary_repeat
                & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80d, 0);
                g_shop_fitting_sell_equipped_result_shown = 0;
            }
        } else if (g_shop_fitting_sell_equipped_prompt_active == 0) {
            g_shop_fitting_sell_equipped_slot
                = menu_update_vertical_selection_and_mark_change(MENU_EQUIPMENT_SLOT_COUNT, SCREEN_CURSOR_REMOVE_SLOT,
                    g_menu_input_primary_repeat, SOUND_SFX_PAGE_SWITCH);
            if (g_formation_unit_pointers[g_unit_selected_index]->equipment[g_shop_fitting_sell_equipped_slot]
                != ITEM_ID_NOTHING) {
                text_id
                    = (g_formation_unit_pointers[g_unit_selected_index]->equipment[g_shop_fitting_sell_equipped_slot]
                          & ITEM_ENTRY_ID_MASK)
                    + HELP_TEXT_ID_ITEM_BASE;
            } else {
                text_id = -1;
            }
            input = g_menu_input_primary_repeat;
            g_help_message_text_id = text_id;
            if (input & PSX_PAD_CIRCLE) {
                item = g_formation_unit_pointers[g_unit_selected_index]->equipment[g_shop_fitting_sell_equipped_slot];
                if (item == ITEM_ID_NOTHING || (item >> 15) != 0) {
                    g_sound_queued_effect_id = SOUND_SFX_INVALID;
                } else {
                    g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                    g_shop_fitting_sell_equipped_prompt_active = 1;
                    g_shop_fitting_sell_equipped_menu.selected_index = 0;
                    g_text_substitution_values[0] = shop_get_item_half_price(item);
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf810, 0);
                }
            } else if (input & PSX_PAD_CROSS) {
                close = 1;
            }
        } else {
            g_help_message_text_id = 0;
            g_sound_suppressed = g_shop_fitting_sell_equipped_prompt_active
                = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_shop_fitting_sell_equipped_menu);
            if (g_shop_fitting_sell_equipped_prompt_active == 0) {
                if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf811, 0);
                    shop_adjust_gil(g_text_substitution_values[0]);
                    g_formation_unit_pointers[g_unit_selected_index]->equipment[g_shop_fitting_sell_equipped_slot]
                        = ITEM_ID_NOTHING;
                    formation_recalculate_selected_stats();
                    g_sound_queued_effect_id = SOUND_SFX_SHOP_TRANSACTION;
                } else {
                    if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 1) {
                        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                    }
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80a, 0);
                }
                g_shop_fitting_sell_equipped_result_shown = 1;
            }
        }
        if (close != 0) {
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_formation_unit_cycle_mode = 2;
            g_shop_fitting_sell_equipped_menu_open = 0;
            g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        g_shop_fitting_sell_equipped_cursor_point.y = g_shop_fitting_sell_equipped_slot * 16 + 0x90;
        gfx_set_otag_index(0x3d);
        screen_draw_sliding_cursor(&g_shop_fitting_sell_equipped_cursor_point,
            &g_shop_fitting_sell_equipped_cursor_anim, menu_input_get_lock_mode(0));
    }
    shop_fitting_draw_cost();
}

/*
 * The fitting room's Sell option (SHOP_STEP_FITTING_ROOM_SELL): run the
 * shop's sell list and sell confirm steps inside the fitting room, after
 * restarting the message box and stopping the unit panels.
 *
 * Those steps set g_shop_menu_step themselves; their result is kept as
 * the sub-step and the shop step put back to FITTING_ROOM_SELL, except that
 * leaving for the main menu returns to the fitting room's unit grid instead.
 */
void shop_fitting_sell_step(void) {
    if (g_shop_fitting_sell_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
            return;
        }
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        g_formation_unit_browse_enabled = 0;
        g_formation_unit_banner_enabled = 0;
        formation_stop_panels();
        g_formation_unit_cycle_mode = 0;
        g_shop_fitting_sell_step = SHOP_STEP_SELL_LIST;
        menu_clear_selection_record(5);
        menu_clear_selection_record(6);
        menu_clear_selection_record(7);
        menu_clear_selection_record(8);
        menu_clear_selection_record(9);
        g_shop_fitting_sell_initialized = 1;
    }
    g_shop_menu_step = g_shop_fitting_sell_step;
    if (g_shop_fitting_sell_step == SHOP_STEP_SELL_LIST) {
        shop_sell_list_step();
        if (g_shop_fitting_sell_step != g_shop_menu_step) {
            screen_reset_selection_results();
        }
    } else if (g_shop_fitting_sell_step == SHOP_STEP_SELL_CONFIRM) {
        shop_sell_confirm_step();
    }
    if (g_shop_menu_step == SHOP_STEP_MAIN_MENU) {
        g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
        g_formation_unit_cycle_mode = 2;
        g_shop_fitting_sell_initialized = 0;
        g_formation_unit_browse_enabled = 1;
        menu_set_vsync_mode_and_event_speed(2);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        return;
    }
    g_shop_fitting_sell_step = g_shop_menu_step;
    g_shop_menu_step = SHOP_STEP_FITTING_ROOM_SELL;
}

/* Clears one equipment slot of a formation unit.  An item the unit owned goes
 * back to the fitting room reserve; an item taken from the shop (bit 15 set)
 * is refunded from the fitting room cost instead. */
static void shop_fitting_return_item(s16 unit_index, s16 equipment_index) {
    s32 equipped_item = g_formation_unit_pointers[unit_index]->equipment[equipment_index];

    if (equipped_item != ITEM_ID_NOTHING) {
        if ((equipped_item >> 15) == 0)
            g_shop_fitting_items[unit_index][equipment_index] = equipped_item;
        else
            shop_fitting_add_cost(-shop_get_item_price(equipped_item & ITEM_ENTRY_ID_MASK));
        g_formation_unit_pointers[unit_index]->equipment[equipment_index] = ITEM_ID_NOTHING;
    }
}

/* Take the tried-on item off one slot and put back what the unit set aside in the fitting room:
 * for a hand slot, both hands' reserved items when both hands are now empty, else the slot's
 * own reserved item if it fits beside the other hand; any other slot always gets its reserved
 * item (possibly nothing) back. The fit check uses g_unit_selected_index, which is what every
 * caller passes as formation_index. */
static void shop_fitting_take_off(s16 formation_index, s16 equipment_index) {
    u16 reserved_item;

    g_formation_unit_pointers[formation_index]->equipment[equipment_index] = ITEM_ID_NOTHING;
    reserved_item = g_shop_fitting_items[formation_index][equipment_index];
    if (equipment_index < MENU_EQUIPMENT_SLOT_HEAD) {
        u16 right_hand = g_formation_unit_pointers[formation_index]->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
        u16 left_hand = g_formation_unit_pointers[formation_index]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND];

        if (right_hand == ITEM_ID_NOTHING && left_hand == ITEM_ID_NOTHING) {
            g_formation_unit_pointers[formation_index]->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND]
                = g_shop_fitting_items[formation_index][MENU_EQUIPMENT_SLOT_RIGHT_HAND];
            g_formation_unit_pointers[formation_index]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND]
                = g_shop_fitting_items[formation_index][MENU_EQUIPMENT_SLOT_LEFT_HAND];
            g_shop_fitting_items[formation_index][MENU_EQUIPMENT_SLOT_RIGHT_HAND] = ITEM_ID_NOTHING;
            g_shop_fitting_items[formation_index][MENU_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
            return;
        }
        if (reserved_item != ITEM_ID_NOTHING
            && formation_item_can_equip_in_slot(g_unit_selected_index, equipment_index, reserved_item) == 1) {
            g_formation_unit_pointers[formation_index]->equipment[equipment_index] = reserved_item;
            g_shop_fitting_items[formation_index][equipment_index] = ITEM_ID_NOTHING;
        }
    } else {
        g_formation_unit_pointers[formation_index]->equipment[equipment_index] = reserved_item;
        g_shop_fitting_items[formation_index][equipment_index] = ITEM_ID_NOTHING;
    }
}

/* Complete a fitting-room purchase: every tried-on item (entry bits 14-15
 * set) becomes the unit's own, and the equipment it displaced, kept in
 * g_shop_fitting_items, goes back to the inventory. */
static void shop_fitting_buy(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_formation_unit_count; i++) {
        for (j = 0; j < MENU_EQUIPMENT_SLOT_COUNT; j++) {
            s32 equipped_item = g_formation_unit_pointers[i]->equipment[j];
            if ((equipped_item >> 14) != 0) {
                g_formation_unit_pointers[i]->equipment[j] &= ITEM_ENTRY_ID_MASK;
            }
            item_adjust_inventory_count(g_shop_fitting_items[i][j], 1);
            g_shop_fitting_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}

/* Fur Shop main menu step: without a unit that has Secret Hunt the shop turns the party away;
 * otherwise Buy (only when poached stock exists), Sell (only when the party has items) or Leave,
 * with a message explaining a refusal. */
void fur_shop_menu_step(void) {
    s32 i;

    if (g_fur_shop_opened == 0) {
        g_menu_list_selected_index = 0;
        menu_set_vsync_mode_and_event_speed(0);
        g_fur_shop_has_secret_hunt = 0;
        g_fur_shop_has_poached_stock = 0;
        g_fur_shop_access_granted = 0;
        g_fur_shop_waiting_message = 0;
        for (i = 0; i < g_formation_unit_count; i++) {
            if (formation_ability_has_secret_hunt(i) != 0) {
                g_fur_shop_has_secret_hunt = 1;
            }
        }
        if (g_fur_shop_has_secret_hunt != 0) {
            for (i = 1; i < 0x100; i++) {
                if (fur_shop_adjust_stock(i, 0) != 0) {
                    g_fur_shop_has_poached_stock = 1;
                }
            }
        }
        if (g_fur_shop_has_secret_hunt == 0) {
            g_shop_menu_step = SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82D, 0);
            return;
        }
        g_fur_shop_access_granted = 1;
        g_fur_shop_opened = 1;
    }
    if (g_fur_shop_waiting_message != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF830, 0);
            g_fur_shop_waiting_message = 0;
        }
    } else if (menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_fur_shop_menu_thread_data) == 0) {
        menu_clear_selection_records();
        if (g_screen_selection_results[0] == 0) {
            if (g_fur_shop_has_poached_stock != 0) {
                g_shop_item_category = screen_init_cursor_position(SCREEN_CURSOR_SHOP_ITEM_TAB, 0);
                g_shop_menu_step = SHOP_STEP_FUR_BUY_LIST;
                g_fur_shop_opened = 0;
            } else {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82E, 0);
                g_fur_shop_waiting_message = 1;
            }
        } else if (g_screen_selection_results[0] == 1) {
            if (formation_has_items(0) != 0) {
                g_shop_menu_step = SHOP_STEP_FUR_SELL_LIST;
                g_fur_shop_opened = 0;
            } else {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF845, 0);
                g_fur_shop_waiting_message = 1;
            }
        } else {
            g_shop_menu_step = SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF831, 0);
            g_fur_shop_opened = 0;
        }
    }
    shop_draw_gil();
}

/* Query the selected poached-stock entry with a 0 quantity adjustment.
 *
 * An empty menu returns 0. */
static s32 fur_shop_get_stock(s32 unused_row) {
    if (g_menu_list_entry_count != 0)
        return fur_shop_adjust_stock(g_item_list_entries[g_menu_list_selected_index], 0);
    return 0;
}

/* Fur shop: the list of the party's own inventory to sell; sibling of
 * fur_shop_buy_list_step.
 *
 * The callback block is written in ascending address order (794 -> 7ac), as
 * the sibling writes it. That makes gcc materialize the two twice-used
 * addresses (count_2 in a0, count in v1) up front at 0xb0-0xbc and defer
 * their stores; the target's emission order then follows. First-use order,
 * not store order, decides this.
 */
void fur_shop_sell_list_step(void) {
    s32 message;

    if (g_fur_shop_sell_list_initialized == 0) {
        message = formation_has_items(0) != 0 ? 0xf840 : 0xf845;

        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, message, 0);
        g_shop_item_category = screen_init_cursor_position(SCREEN_CURSOR_SHOP_ITEM_TAB, 0);
        formation_item_build_list(-1, g_shop_item_category + 1,
            g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_init_scrollable_list_core(g_item_list_entries, 0, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[1].value = equipment_get_row_item_count;
        g_menu_row_callbacks[2].icon = item_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
        g_menu_row_callbacks[4].value = shop_sell_get_price;
        g_menu_row_callbacks[5].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[6].value = equipment_get_row_item_count;
        g_fur_shop_sell_list_initialized = 1;
    }

    menu_set_vsync_mode_and_event_speed(2);
    if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) != 0) {
        g_fur_shop_sell_list_initialized = 0;
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
    menu_run_script_with_palette_mode(&g_shop_sell_confirm_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    shop_draw_gil();
}

/* Fur shop: confirm a sale. The party receives half price, and the sold copies join the
 * poached stock. */
void fur_shop_sell_confirm_step(void) {
    s32 result = 1;

    if (g_fur_shop_sale_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_fur_shop_sale_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF841, 0);
            g_shop_menu_step = SHOP_STEP_FUR_SELL_LIST;
            g_screen_prompt_active = 0;
        }
    } else {
        result = fur_shop_sell_quantity_prompt();
    }
    if (result == 0) {
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            shop_adjust_gil(shop_get_item_half_price(g_item_list_entries[g_menu_list_selected_index])
                * g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                -g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            fur_shop_adjust_stock(g_item_list_entries[g_menu_list_selected_index],
                g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF837, 0);
            item_rebuild_category_list(g_shop_item_category);
            formation_item_build_list(0, g_shop_item_category + 1,
                g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            screen_init_list(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_shop_sell_confirm_script);
            menu_set_window_scale_step(10);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_fur_shop_sale_message_wait = 1;
    }
    shop_switch_item_tab(0);
    menu_run_script_with_palette_mode(&g_shop_sell_confirm_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    shop_draw_gil();
}

/* Fur shop: the quantity prompt of a sale, capped by the inventory count. */
static s32 fur_shop_sell_quantity_prompt(void) {
    s32 count;

    if (g_fur_shop_quantity_menu_running == 0) {
        g_shop_sell_total_shown = 0;
        g_shop_sell_quantity_item = g_item_list_entries[g_menu_list_selected_index];
        g_screen_prompt_active = 1;
        count = item_adjust_inventory_count(g_shop_sell_quantity_item, 0);
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF842, 0);
            return -1;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF843, 0);
        g_shop_quantity_limit = count;
        g_shop_sell_total_shown = 0;
        g_sound_suppressed = 1;
    }
    g_fur_shop_quantity_menu_running
        = menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, g_shop_quantity_menu_data);
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_shop_sell_total_shown == 0) {
        g_text_substitution_values[0] = shop_get_item_half_price(g_shop_sell_quantity_item)
            * g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY];
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF844, 0);
        g_shop_sell_total_shown = 1;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_shop_sell_total_shown != 0) {
        g_shop_sell_total_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF843, 0);
    }
    if (g_fur_shop_quantity_menu_running == 0) {
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = SOUND_SFX_SHOP_TRANSACTION;
        }
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_fur_shop_quantity_menu_running;
}

/* Price column of the Fur Shop buy list: half the item's price, flagged disabled when the entry is
 * unequippable. */
static s32 fur_shop_buy_get_price(s32 entry_index) {
    s32 display_value = shop_get_item_price(g_item_list_entries[entry_index]) >> 1;

    if ((g_item_list_entries[entry_index] & ITEM_ENTRY_UNEQUIPPABLE) != 0)
        display_value |= MENU_NUMBER_VALUE_DISABLED;
    return display_value;
}

/* Fur shop: the list of the shop's poached stock, bought at half price.
 *
 * Mirrors shop_buy_list_step with the half-price value and
 * the poached-stock quantity as callbacks. */
void fur_shop_buy_list_step(void) {
    s16 x;
    s16 y;
    s32 input;
    s32 disabled;
    s8 result;

    if (g_fur_shop_purchase_list_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF832, 0);
        g_fur_shop_purchase_list_initialized = 1;
        shop_build_stock_list(-1, g_screen_context_id,
            g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_get_selection_record(g_shop_item_category, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[1].value = equipment_get_row_item_count;
        g_menu_row_callbacks[2].icon = item_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
        g_menu_row_callbacks[4].value = fur_shop_buy_get_price;
        g_menu_row_callbacks[5].value = shop_buy_get_equipped_count;
        g_menu_row_callbacks[6].value = shop_buy_get_owned_count;
        g_menu_row_callbacks[8].value = fur_shop_get_stock;
        g_fur_shop_equip_check_pending = 0;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (g_fur_shop_equip_check_pending != 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        result = thread_is_running(THREAD_SCREEN_SLOT_MESSAGE);
        g_fur_shop_equip_check_pending = result;
        if (result == 0) {
            menu_set_selection_record(
                g_shop_item_category, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            g_shop_menu_step = SHOP_STEP_FUR_BUY_EQUIP;
            g_fur_shop_purchase_list_initialized = 0;
        }
        menu_input_clear_state();
    } else {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CROSS) {
            g_fur_shop_purchase_list_initialized = 0;
            g_shop_menu_step = SHOP_STEP_FUR_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF830, 0);
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        } else if ((input & PSX_PAD_TRIANGLE) && g_menu_list_entry_count != 0
            && g_shop_item_category != ITEM_MENU_TAB_OTHER) {
            g_fur_shop_equip_check_pending = 1;
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
    menu_run_script_with_palette_mode(
        &g_fur_shop_purchase_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    if (screen_is_busy() != 0 || g_menu_list_entry_count == 0 || g_shop_item_category == ITEM_MENU_TAB_OTHER) {
        disabled = 1;
    }
    menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, SOUND_SFX_CONFIRM);
    shop_draw_gil();
}

/* Fur shop sibling of shop_buy_confirm_step: the party pays half
 * price and the poached stock is reduced. */
void fur_shop_buy_confirm_step(void) {
    s32 result = 1;

    if (g_fur_shop_purchase_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_fur_shop_purchase_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF83B, 0);
            g_shop_menu_step = SHOP_STEP_FUR_BUY_LIST;
            g_screen_prompt_active = 0;
        }
    } else {
        result = fur_shop_buy_quantity_prompt();
    }
    if (result == 0) {
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            shop_adjust_gil(-(shop_get_item_price(g_item_list_entries[g_menu_list_selected_index]) >> 1)
                * g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            fur_shop_adjust_stock(g_item_list_entries[g_menu_list_selected_index],
                -g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            item_rebuild_category_list(g_shop_item_category);
            shop_build_stock_list(-1, g_screen_context_id,
                g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            screen_init_list(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_fur_shop_purchase_menu_script);
            menu_set_window_scale_step(10);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF837, 0);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_fur_shop_purchase_message_wait = 1;
    }
    shop_switch_item_tab(0);
    menu_run_script_with_palette_mode(&g_fur_shop_purchase_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C, 0, 1, SOUND_SFX_CONFIRM);
    shop_draw_gil();
}

/* Fur shop twin of shop_buy_quantity_prompt: the quantity
 * prompt is capped by the 99-item inventory limit, the gil on hand at half
 * price and the poached stock. */
static s32 fur_shop_buy_quantity_prompt(void) {
    s32 owned;
    s32 count;
    s32 price;
    s32 room;
    s32 stock;

    if (g_fur_shop_purchase_result == 0) {
        g_fur_shop_purchase_confirm_shown = 0;
        g_fur_shop_purchase_item = g_item_list_entries[g_menu_list_selected_index];
        g_screen_prompt_active = 1;
        owned = item_get_total_count(g_fur_shop_purchase_item);
        if (owned >= 99) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF834, 0);
            return SHOP_QUANTITY_HOLDING_MAX;
        }
        count = shop_adjust_gil(0);
        price = shop_get_item_price(g_fur_shop_purchase_item) >> 1;
        count /= price;
        g_fur_shop_purchase_price = price;
        room = 99 - owned;
        if (room < count) {
            count = room;
        }
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF833, 0);
            return SHOP_QUANTITY_CANNOT_AFFORD;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF835, 0);
        stock = fur_shop_adjust_stock(g_fur_shop_purchase_item, 0);
        if (count >= stock) {
            count = stock;
        }
        g_shop_quantity_limit = count;
        g_sound_suppressed = 1;
    }
    g_fur_shop_purchase_result = menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, g_shop_quantity_menu_data);
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_fur_shop_purchase_confirm_shown == 0) {
        g_text_substitution_values[0]
            = g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY] * g_fur_shop_purchase_price;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF836, 0);
        g_fur_shop_purchase_confirm_shown = 1;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_fur_shop_purchase_confirm_shown != 0) {
        g_fur_shop_purchase_confirm_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF835, 0);
    }
    if (g_fur_shop_purchase_result == 0) {
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = SOUND_SFX_SHOP_TRANSACTION;
        }
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_fur_shop_purchase_result;
}

/* Fur Shop equip-on-purchase step: run the shared equip-candidate step and return to the Fur Shop
 * buy list instead of the normal one. */
void fur_shop_buy_equip_step(void) {
    shop_buy_equip_step();
    if (g_shop_menu_step == SHOP_STEP_BUY_LIST)
        g_shop_menu_step = SHOP_STEP_FUR_BUY_LIST;
}

/* Soldier Office main step: runs the hire menu. Rows 0-1 generate a recruit of the chosen
 * kind, whose fee is 1000 gil plus its starting equipment's price (when the party cannot pay,
 * the recruit is removed again); row 2 renames a monster; row 3 or a cancel leaves, after
 * cashing out inventory beyond 99 of an item. */
void soldier_office_menu_step(void) {
    unit_stats_t unit;
    s32 i;
    s32 result;
    s32 slot;

    if (g_soldier_office_hire_menu_initialized == 0) {
        g_soldier_office_hire_confirm_menu.selected_index = 0;
        g_soldier_office_hire_fee_confirm_menu.selected_index = 0;
        g_soldier_office_hire_result_shown = 0;
        g_soldier_office_hire_menu_initialized = 1;
        g_unit_panel_status_display_thread_params.y = 0;
        g_soldier_office_hire_unread_flag_1 = 0;
        g_soldier_office_hire_unread_flag_2 = 0;
        g_formation_unit_banner_enabled = 0;
        g_soldier_office_unit_count = 0;
        for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
            result = unit_init_from_party_slot(&unit, i, 1);
            if (result == 0 && unit.formation_index < PARTY_GUEST_SLOT_FIRST) {
                g_soldier_office_unit_count++;
            }
        }
    }
    if (g_soldier_office_hire_result_shown != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        }
    } else {
        i = menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, &g_soldier_office_hire_menu_thread_data);
        if (i == 0) {
            if (g_screen_selection_results[0] == 3 || g_screen_selection_results[0] == -1) {
                g_soldier_office_hire_menu_initialized = 0;
                g_shop_menu_step = SHOP_STEP_EXIT_FADE;
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF825, 0);
                shop_cash_out_excess_items();
            } else if (g_screen_selection_results[0] == 2) {
                g_formation_unit_count
                    = formation_build_record_list(0, g_formation_unit_pointers, FORMATION_RECORD_LIST_MONSTERS);
                if (g_formation_unit_count != 0) {
                    g_shop_menu_step = SHOP_STEP_RENAME_SELECT_UNIT;
                    i = 0xF847;
                } else {
                    i = 0xF846;
                    g_soldier_office_hire_result_shown = 1;
                }
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, i, 0);
            } else {
                slot = formation_generate_unit(g_screen_selection_results[0]);
                if (slot >= 0) {
                    g_soldier_office_fee = 1000;
                    /* `i` is still 0, the menu thread's result. */
                    for (; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                        g_soldier_office_fee += shop_get_item_price(g_formation_unit_pointers[slot]->equipment[i]);
                    }
                    if (shop_adjust_gil(0) < g_soldier_office_fee) {
                        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF823, 0);
                        party_remove_unit(g_formation_unit_pointers[slot]->identity.roster_slot);
                        g_soldier_office_hire_result_shown = 1;
                    } else {
                        g_unit_selected_index = slot;
                        formation_stage_selected_unit();
                        g_formation_unit_banner_at_bottom = 0;
                        g_formation_unit_banner_at_bottom_latch = 0;
                        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF824, 0);
                        g_shop_menu_step = SHOP_STEP_HIRE_PREVIEW;
                    }
                } else {
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF822, 0);
                    g_soldier_office_hire_result_shown = 1;
                }
            }
        }
    }
    shop_draw_gil();
    soldier_office_draw_fee();
    soldier_office_draw_unit_count();
}

/*
 * Soldier office step that shows the generated recruit.
 *
 * Any face button dismisses the message box; once it has closed, unit
 * browsing and the status banner are turned on for the preview. A further
 * face button closes the panels and moves on to the hire confirmation
 * (message 0xF826).
 */
void soldier_office_hire_preview_step(void) {
    if (g_soldier_office_hire_preview_initialized == 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_formation_unit_browse_enabled = 1;
            g_formation_unit_banner_enabled = 1;
            g_formation_unit_banner_at_bottom = 0;
            g_unit_panel_status_display_thread_params.y = 0;
            g_help_message_text_id = MENU_GRID_HELP_RECRUIT_PREVIEW;
            g_soldier_office_hire_preview_initialized = 1;
        }
        shop_draw_gil();
        soldier_office_draw_fee();
        soldier_office_draw_unit_count();
    } else if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
        g_shop_menu_step = SHOP_STEP_HIRE_CONFIRM;
        g_formation_unit_browse_enabled = 0;
        g_formation_unit_banner_enabled = 0;
        unit_panel_toggle_numbers_thread(0);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF826, 0);
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        g_soldier_office_hire_preview_initialized = 0;
    }
}

/*
 * Soldier office: confirm hiring the previewed recruit.
 *
 * Selection 1 or -1 declines and removes the recruit from the roster;
 * selection 2 returns to the preview. Any other result shows a message and
 * waits for its thread before going on to the name entry.
 */
void soldier_office_hire_confirm_step(void) {
    s16 selection;

    if (g_soldier_office_hire_confirm_message_wait != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_soldier_office_hire_confirm_message_wait = 0;
            g_shop_menu_step = SHOP_STEP_HIRE_NAME_ENTRY;
        }
    } else if (menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, &g_soldier_office_hire_confirm_menu) == 0) {
        selection = g_screen_selection_results[2];
        if (selection == 1 || selection == -1) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF827, 0);
            g_soldier_office_hire_menu_initialized = 0;
            party_remove_unit(g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot);
            g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_MENU;
        } else if (selection == 2) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_shop_menu_step = SHOP_STEP_HIRE_PREVIEW;
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF829, 0);
            g_soldier_office_hire_confirm_message_wait = 1;
        }
    }
    shop_draw_gil();
    soldier_office_draw_fee();
    soldier_office_draw_unit_count();
}

/* Soldier office: the name entry keyboard for the recruit, then the fee prompt. */
void soldier_office_hire_name_step(void) {
    if (g_soldier_office_hire_name_entry_initialized == 0) {
        g_formation_unit_banner_enabled = 0;
        unit_panel_toggle_banner_thread(0);
        g_soldier_office_hire_name_entry_initialized = 1;
    }
    g_soldier_office_entered_name
        = name_entry_keyboard_frame(g_formation_unit_pointers[g_unit_selected_index]->name, 0);
    if (g_soldier_office_entered_name != 0) {
        g_soldier_office_hire_name_entry_initialized = 0;
        g_text_substitution_values[0] = g_soldier_office_fee;
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82A, 0);
        g_shop_menu_step = SHOP_STEP_HIRE_FEE_CONFIRM;
    }
}

/*
 * Soldier office: confirm the hiring fee.
 *
 * Choice 0 pays the fee and stores the recruit's name; 1 or -1 removes the
 * recruit from the roster. Other choices return to the name entry.
 */
void soldier_office_hire_fee_step(void) {
    s16 choice;

    g_help_message_text_id = 0;
    if (g_soldier_office_hire_fee_message_wait != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_REOPEN;
            g_soldier_office_hire_fee_message_wait = 0;
        }
    } else if (menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, &g_soldier_office_hire_fee_confirm_menu) == 0) {
        choice = g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT];
        if (choice == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf82b, 0);
            shop_adjust_gil(-g_soldier_office_fee);
            formation_set_unit_name(g_unit_selected_index, g_soldier_office_entered_name);
            g_soldier_office_hire_fee_message_wait = 1;
        } else if (choice == 1 || choice == -1) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf827, 0);
            g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_MENU;
            party_remove_unit(g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot);
            g_soldier_office_hire_menu_initialized = 0;
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_soldier_office_hire_fee_confirm_menu.selected_index = 0;
            g_shop_menu_step = SHOP_STEP_HIRE_NAME_ENTRY;
        }
    }
    shop_draw_gil();
    soldier_office_draw_fee();
    soldier_office_draw_unit_count();
}

/* Return to the soldier office menu with message 0xf821, redrawing the gil,
 * fee and roster-count displays for this frame. */
void soldier_office_reopen_step(void) {
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf821, 0);
    g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_MENU;
    g_soldier_office_hire_menu_initialized = 0;
    shop_draw_gil();
    soldier_office_draw_fee();
    soldier_office_draw_unit_count();
}

/* Menu callback: no marker (-1) for any unit. */
static s32 soldier_office_rename_get_unit_marker(s32 unused_unit_index) {
    return -1;
}

/*
 * Soldier Office rename step: waits for a face button to dismiss the
 * message, then shows the monster grid. Circle opens the name entry for the
 * selected unit, except that Reis in dragon form refuses (message 0xF848, in
 * a box placed away from the unit banner); Cross returns to the Soldier
 * Office menu.
 */
void soldier_office_rename_unit_step(void) {
    s32 mode;
    s32 input;
    s32 text_id;

    if (g_soldier_office_rename_prompt_initialized == 0) {
        if (!(g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE))) {
            shop_draw_gil();
            soldier_office_draw_fee();
            soldier_office_draw_unit_count();
            return;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_soldier_office_rename_error_shown = 0;
        g_soldier_office_rename_prompt_initialized = 1;
        formation_init_grid_cursor(0);
        g_shop_background_visible = 0;
    }
    if (g_formation_unit_banner_enabled != 0) {
        text_id = MENU_GRID_HELP_BANNER_TOP;
        if (g_formation_unit_banner_at_bottom != 0) {
            text_id = MENU_GRID_HELP_BANNER_BOTTOM;
        }
        g_help_message_text_id = text_id;
    } else {
        g_help_message_text_id = 0;
    }
    formation_update_grid(
        1, 1, 0, g_menu_input_secondary_repeat, g_menu_input_lock_mode, soldier_office_rename_get_unit_marker, 0);
    if (g_soldier_office_rename_error_shown != 0) {
        g_help_message_text_id = -1;
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_soldier_office_rename_error_shown = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0 && formation_is_grid_scroll_needed() == 0) {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CIRCLE) {
            if (g_formation_unit_pointers[g_unit_selected_index]->sprite_set == CHARACTER_IDENTITY_REIS_DRAGON) {
                if (g_formation_unit_banner_at_bottom != 0) {
                    mode = 1;
                } else {
                    mode = 2;
                }
                thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, mode | 0x18, 0xF848, 0);
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
                g_soldier_office_rename_error_shown = 1;
            } else {
                g_shop_menu_step = SHOP_STEP_RENAME_NAME_ENTRY;
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                g_formation_unit_banner_enabled = 0;
                g_soldier_office_rename_prompt_initialized = 0;
            }
        } else if (input & PSX_PAD_CROSS) {
            g_shop_background_visible = 1;
            g_soldier_office_rename_prompt_initialized = 0;
            g_formation_unit_banner_enabled = 0;
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF821, 0);
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        }
    }
}

/* Soldier office: the name entry keyboard of the Rename service; sibling of
 * soldier_office_hire_name_step, which goes on to the fee prompt (0xF82A). */
void soldier_office_rename_name_step(void) {
    if (g_soldier_office_rename_name_entry_initialized == 0) {
        g_formation_unit_banner_enabled = 0;
        unit_panel_toggle_banner_thread(0);
        g_soldier_office_rename_name_entry_initialized = 1;
    }
    formation_update_grid(1, 0, 0, 0, g_menu_input_lock_mode, soldier_office_rename_get_unit_marker, 0);
    g_soldier_office_entered_name
        = name_entry_keyboard_frame(g_formation_unit_pointers[g_unit_selected_index]->name, 0);
    if (g_soldier_office_entered_name != 0) {
        g_shop_menu_step = SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        formation_set_unit_name(g_unit_selected_index, g_soldier_office_entered_name);
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF821, 0);
        g_shop_background_visible = 1;
        g_soldier_office_rename_name_entry_initialized = 0;
    }
}

/* Menu callback: the hiring fee shown for the highlighted row of the hire menu,
 * 1500 for row 0, 1400 for row 1 and 0 otherwise. The fee actually charged is
 * computed when the recruit is generated (soldier_office_menu_step). */
static s32 soldier_office_get_fee(s32 unused_row) {
    s16 mode = g_soldier_office_hire_menu_thread_data.selected_index;

    if (mode == 0) {
        return 1500;
    }
    return (mode == 1) ? 1400 : 0;
}

/* Run the soldier office's fee display script with
 * soldier_office_get_fee installed as menu value callback 0,
 * then restore the previous callback. */
static void soldier_office_draw_fee(void) {
    menu_callback_t* callback_slot;
    s32 (*previous_callback)(s32);

    callback_slot = g_menu_row_callbacks;
    previous_callback = callback_slot->value;
    callback_slot->value = soldier_office_get_fee;
    menu_run_script_with_palette_mode(&g_soldier_office_fee_script, 0, 0);
    callback_slot->value = previous_callback;
}

/* Display the party unit count directly while value callback 0 is disabled. */
static void soldier_office_draw_unit_count(void) {
    s32 (*previous_callback)(s32);

    previous_callback = g_menu_row_callbacks[0].value;
    g_menu_row_callbacks[0].value = 0;
    g_soldier_office_unit_count_script.unit_count.value = g_soldier_office_unit_count;
    /* The serialized script prefix shares the bytecode interpreter's instruction view. */
    menu_run_script_with_palette_mode((menu_instruction_t*)&g_soldier_office_unit_count_script, 0, 0);
    g_menu_row_callbacks[0].value = previous_callback;
}
