/* The shop, including the main, gil, purchase, sell, fitting room, best fit preview and fur shop menus and steps. */

#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static void world_menu_run_script_with_callback_suppressed(void);
static void world_shop_apply_fitting_room_items_to_all_units(void);
static void world_shop_buy_from_fitting_room(void);
static void world_shop_finalize_unit_equips_from_fitting_room(s16 formation_index, s16 equipment_index);
static void world_shop_install_callbacks_and_run(void);
static void world_shop_return_unit_equip_to_fitting_room(s16 unit_index, s16 equipment_index);
static s32 world_shop_run_fitting_room_checkout_prompt(void);
static s32 world_shop_run_fur_buy_quantity_menu(void);
static s32 world_shop_run_fur_sell_quantity_menu(void);
static s32 world_shop_run_item_purchase_quantity_menu(void);
static s32 world_shop_run_item_sell_menu(void);
static void world_shop_run_obtain_gil_menu(void);
static void world_shop_run_soldier_office_fee_menu_script(void);

void world_shop_run_screen(s32 shop_type) {
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
    RECT* tile_box = &g_world_shop_edge_tile_rect;
    s32 buttons;
    s32 ot_index;
    s32 disabled;
    s32 tim_index;
    s16 last_step;

    g_world_ot_length = 0x40;
    g_world_gfx_textured_quad_capacity = 0x1F4;
    g_gfx_poly_f3_capacity = 0;
    g_gfx_poly_ft3_capacity = 0;
    g_gfx_poly_f4_capacity = 0;
    g_gfx_poly_g3_capacity = 0;
    g_gfx_poly_gt3_capacity = 0;
    g_world_gfx_gradient_quad_capacity = 0x5A;
    g_world_gfx_textured_gradient_quad_capacity = 0x3C;
    g_world_gfx_flat_line_capacity = 0xA;
    g_gfx_line_f3_capacity = 0;
    g_gfx_line_f4_capacity = 0;
    g_world_gfx_gradient_line_capacity = 0;
    g_world_gfx_line_g3_capacity = 0;
    g_gfx_line_g4_capacity = 0;
    g_gfx_tile_capacity = 0x1E;
    g_gfx_tile_1_capacity = 0;
    g_gfx_tile_8_capacity = 0;
    g_gfx_tile_16_capacity = 0;
    g_gfx_sprite_capacity = 0;
    g_gfx_sprite_8_capacity = 0;
    g_gfx_sprite_16_capacity = 0;
    g_world_gfx_draw_move_capacity = 0x1E;
    g_world_gfx_draw_area_capacity = 0xA;
    g_gfx_draw_mode_capacity = 0x1E;
    g_world_menu_context_id = shop_type;
    buttons = script_get_variable(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
    if (shop_type == WORLD_MENU_CONTEXT_FUR_SHOP) {
        ot_index = MAIN_MUSIC_FUR_SHOP;
        g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_MENU;
        disabled = 0xF82F;
        tim_index = 3;
    } else if (shop_type == WORLD_MENU_CONTEXT_SOLDIER_OFFICE) {
        ot_index = MAIN_MUSIC_SOLDIER_OFFICE;
        g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
        disabled = 0xF820;
        tim_index = 6;
    } else {
        g_world_menu_context_id = script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
        ot_index = MAIN_MUSIC_SHOP;
        disabled = 0xF801;
        tim_index = 0;
        g_world_shop_menu_step = WORLD_SHOP_STEP_MAIN_MENU;
    }
    SetDispMask(0);
    while (DrawSync(1) != 0) { }
    gfx_move_image_and_wait(&g_world_shop_background_vram_rect, 0, 0);
    tim = world_shop_load_background_tim(tim_index + buttons);
    gfx_load_image_and_wait(&g_world_shop_background_vram_rect, tim + 5);
    gfx_store_image_and_wait(&g_world_shop_vram_backup_rect, tim);
    world_gfx_init_double_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, gradient_quads,
        textured_gradient_quads, flat_lines, 0, 0, 0, 0, 0, tiles, 0, 0, 0, 0, 0, 0, draw_moves, draw_areas,
        draw_modes);
    last_step = -1;
    world_formation_init_menu_state();
    g_world_shop_main_menu_thread_data.selected_index = 0;
    g_world_shop_fur_menu_thread_data.selected_index = 0;
    g_world_shop_hire_menu_thread_data.selected_index = 0;
    world_text_bind_section_data(WORLD_TEXT_DATA_SHOP_DIALOGUE);
    g_text_section_pointers[WORLD_TEXT_SECTION_SHOP_MENU_LABELS] = g_world_text_shop_menu_labels_data;
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, disabled, 0);
    g_world_menu_music_slot = main_sound_open_music_into_free_slot(ot_index);
    main_sound_switch_music_track(g_world_menu_music_slot, 0x7F, 0);
    g_world_shop_background_visible = 1;
    g_world_formation_unit_banner_enabled = 0;
    while (g_world_shop_menu_step != WORLD_SHOP_STEP_EXIT) {
        world_gfx_present_frame_and_swap_packet_buffer(0, 0);
        menu_input_update_with_message_state_variant();
        world_menu_update_thread_7_idle_countdown();
        world_gfx_update_fade_in_tile();
        if (g_menu_input_primary_repeat & PSX_PAD_SELECT) {
            world_menu_start_description_text_thread(DIALOG_POSITION_CENTER);
        }
        if (g_world_grid_menu_id != MENU_GRID_HELP_NONE) {
            world_menu_update_grid_cursor(g_world_unit_pointers[g_unit_selected_index]->uses_monster_skillset);
        }
        world_menu_set_palette_mode(g_menu_input_lock_mode);
        if (g_world_shop_background_visible != 0) {
            menu_gfx_enqueue_textured_quad(&g_world_shop_background_rect, 0, 0, 0, 0, 0x14C, 0, 1);
            gfx_enqueue_tile(&g_world_shop_backdrop_tile_rect, &g_world_gfx_black_tile_color, 0, 0);
            gfx_enqueue_tile(tile_box, &g_world_gfx_black_tile_color, 0, g_world_ot_length - 1);
        }
        if ((g_world_formation_unit_cycle_mode != 0 || g_world_formation_unit_browse_enabled != 0)
            && g_world_shop_menu_step < 0x14) {
            disabled = 0;
            if (g_menu_input_lock_mode != 0 || g_world_grid_menu_id != MENU_GRID_HELP_NONE
                || g_world_formation_unit_cycle_mode == 1) {
                disabled = 1;
            }
            buttons = 0;
            if (disabled == 0) {
                buttons = g_menu_input_secondary_repeat;
            }
            if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0) {
                if (buttons & PSX_PAD_L1) {
                    g_unit_selected_index
                        = g_unit_selected_index == 0 ? g_world_formation_unit_count - 1 : g_unit_selected_index - 1;
                    buttons &= ~PSX_PAD_R1;
                } else if (buttons & PSX_PAD_R1) {
                    g_unit_selected_index
                        = g_unit_selected_index == g_world_formation_unit_count - 1 ? 0 : g_unit_selected_index + 1;
                }
            } else {
                buttons = 0;
            }
            world_menu_draw_pressable_button(
                MENU_PRESSABLE_BUTTON_L1, 0xC, 0xC, buttons & PSX_PAD_L1, disabled, MAIN_SFX_PAGE_SWITCH);
            world_menu_draw_pressable_button(
                MENU_PRESSABLE_BUTTON_R1, 0xE0, 0xC, buttons & PSX_PAD_R1, disabled, MAIN_SFX_PAGE_SWITCH);
        }
        g_world_shop_menu_step_handlers[g_world_shop_menu_step]();
        if (g_world_shop_menu_step != WORLD_SHOP_STEP_RENAME_NAME_ENTRY
            && g_world_shop_menu_step != WORLD_SHOP_STEP_HIRE_NAME_ENTRY
            && g_world_shop_menu_step != WORLD_SHOP_STEP_FITTING_ROOM_REMOVE) {
            world_shop_update_unit_selection();
        }
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 63;
        menu_draw_active_window_frames();
        if (g_world_shop_menu_step == WORLD_SHOP_STEP_FUR_BUY_CONFIRM
            || g_world_shop_menu_step == WORLD_SHOP_STEP_FUR_SELL_CONFIRM
            || g_world_shop_menu_step == WORLD_SHOP_STEP_FITTING_ROOM_SELL
            || g_world_shop_menu_step == WORLD_SHOP_STEP_BUY_CONFIRM
            || g_world_shop_menu_step == WORLD_SHOP_STEP_SELL_CONFIRM || g_world_shop_cost_window_visible != 0
            || shop_type == WORLD_MENU_CONTEXT_SOLDIER_OFFICE) {
            ot_index = 0x3C;
        } else {
            ot_index = 9;
        }
        menu_input_update_event_state_variant(g_menu_gfx_context->otag + ot_index, world_input_filter_menu(),
            g_world_gfx_draw_buffer_clip_y == 0 ? 0xF0 : 0);
        if (g_world_shop_menu_step != last_step) {
            g_world_formation_unit_cycle_mode = 0;
            menu_set_vsync_mode_and_event_speed(0);
            g_menu_help_message_text_id = 0;
            world_menu_reset_selection_results();
            last_step = g_world_shop_menu_step;
        }
        world_sound_dispatch_effect();
    }
    main_sound_unload_scenario_mus(g_world_menu_music_slot);
    world_formation_save_records_to_party();
    menu_set_vsync_mode_and_event_speed(0);
    world_formation_reset_menu_context();
    gfx_load_image_and_wait(&g_world_shop_background_vram_rect, tim);
    main_heap_free(tim);
}

/* The on_change callback of the shop's quantity range (the record at 0x801950d8 whose max is
 * g_world_shop_quantity_limit): nothing to do. */
void world_shop_menu_noop_callback(void) {
}

/* Shop main menu step: runs the Buy / Sell / Fitting Room menu and, when it
 * closes, moves to the chosen step (rows 0..2; the fitting room also sets its
 * message), or to the exit fade with its message for anything else. */
void world_shop_run_main_menu_step(void) {
    u8 running;

    if (g_world_shop_main_menu_running == 0) {
        g_world_formation_unit_cycle_mode = 0;
        g_menu_list_selected_index = 0;
    }
    running = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_shop_main_menu_thread_data);
    g_world_shop_main_menu_running = running;
    if (running == 0) {
        menu_clear_selection_records();
        if (g_world_menu_selection_results[0] == 0) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_LIST;
            g_world_shop_item_category = world_menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
        } else if (g_world_menu_selection_results[0] == 1) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_SELL_LIST;
        } else if (g_world_menu_selection_results[0] == 2) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
            g_unit_selected_index = 0;
            world_formation_stage_selected_unit();
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF818, 0);
        } else {
            g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF803, 0);
        }
    }
    world_shop_run_obtain_gil_menu();
}

/* Menu screen step: on the first frame reset the fade flag; once any of the
 * face buttons (0xf0) is pressed start the fade-out with a music fade, then
 * wait for the fade to complete before releasing the screen (g_world_shop_menu_step = -1). */
void world_menu_run_exit_fade_step(void) {
    if (g_world_shop_exit_step_initialized == 0) {
        g_world_shop_exit_fade_started = 0;
        g_world_shop_exit_step_initialized = 1;
    }
    if (g_world_shop_exit_fade_started != 0) {
        if (world_gfx_update_fade_out_tile() == 0) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT;
            g_world_shop_exit_step_initialized = 0;
        }
    } else if (g_menu_input_newly_pressed & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
        world_gfx_start_increasing_fade();
        main_sound_set_current_music_target(0, 0xF0);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_world_shop_exit_fade_started = 1;
    }
    world_shop_run_obtain_gil_menu();
    if (g_world_menu_context_id == WORLD_MENU_CONTEXT_SOLDIER_OFFICE) {
        world_shop_run_soldier_office_fee_menu_script();
        world_menu_run_script_with_callback_suppressed();
    }
}

static void world_shop_run_obtain_gil_menu(void) {
    menu_callback_t* callback_slot;
    s32 (*previous_callback)(s32);

    callback_slot = g_world_menu_script_callbacks;
    previous_callback = callback_slot->value;
    callback_slot->value = world_shop_adjust_gil;
    menu_run_script_with_palette_mode_variant(&g_world_shop_obtain_gil_menu_script, 0, 0);
    callback_slot->value = previous_callback;
}

/* Returns the owned-and-equipped count for the selected item. */
static s32 world_item_count_selected_owned_and_equipped(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count != 0) {
        result = item_get_total_count_variant(g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}

/* Returns the party-equipped count for the selected item. */
static s32 world_item_count_selected_equipped_by_party(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count != 0) {
        result = item_get_equipped_count_variant(g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}

/* Full-price twin of world_menu_get_item_half_price_display_value: the menu
 * item's price with bit 30 copied from its 0x4000 entry flag. */
static s32 world_menu_get_item_price_display_value(s32 entry_index) {
    s32 display_value = world_item_get_price(g_item_list_entries[entry_index]);

    if ((g_item_list_entries[entry_index] & WORLD_ITEM_LIST_ENTRY_UNEQUIPPABLE) != 0)
        display_value |= MENU_NUMBER_VALUE_DISABLED;
    return display_value;
}

/* Switch the active item category and restore its saved list cursor. */
static void world_menu_switch_item_category(s32 input) {
    s16 x;
    s16 y;
    s32 category;
    s32 cursor_slot;
    s16 item;
    s32 description;

    category = menu_update_horizontal_selection_and_mark_change(
        5, WORLD_MENU_CURSOR_SHOP_ITEM_TAB, input, MAIN_SFX_PAGE_SWITCH);
    cursor_slot = g_world_shop_item_category;
    if (cursor_slot != category) {
        if (g_world_shop_menu_step != WORLD_SHOP_STEP_BUY_LIST
            && g_world_shop_menu_step != WORLD_SHOP_STEP_FUR_BUY_LIST) {
            cursor_slot += 5;
        }
        menu_set_selection_record(
            cursor_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_world_shop_item_category = category;
        if (g_world_shop_menu_step == WORLD_SHOP_STEP_BUY_LIST
            || g_world_shop_menu_step == WORLD_SHOP_STEP_FUR_BUY_LIST) {
            world_shop_build_item_list(-1, g_world_menu_context_id,
                category != 4 ? (category + 1) : ITEM_LIST_FILTER_OTHER, g_item_list_entries, ITEM_LIST_MODE_ALL);
            cursor_slot = g_world_shop_item_category;
        } else {
            world_item_build_category_list(-1, g_world_shop_item_category + 1,
                category != 4 ? category + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries, ITEM_LIST_MODE_ALL);
            cursor_slot = g_world_shop_item_category + 5;
        }
        menu_get_selection_record_variant(cursor_slot, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
    }
    world_menu_draw_row_window_and_cursor(1, g_world_shop_item_category, g_menu_input_lock_mode);
    item = g_item_list_entries[g_menu_list_selected_index];
    if (item != -1) {
        description = (item & WORLD_ITEM_ID_MASK) + 0x6800;
    } else {
        description = 0;
    }
    g_menu_help_message_text_id = description;
}

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
        menu_get_selection_record_variant(g_world_shop_item_category, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
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
            menu_set_selection_record(
                g_world_shop_item_category, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
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
        } else if ((input & PSX_PAD_TRIANGLE) && g_menu_list_entry_count != 0 && g_world_shop_item_category != 4) {
            g_world_shop_purchase_equip_check_pending = 1;
        } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0) {
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
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_purchase_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    if (world_map_is_busy() != 0 || g_menu_list_entry_count == 0 || g_world_shop_item_category == 4) {
        disabled = 1;
    }
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
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
void world_menu_confirm_item_purchase(void) {
    s32 result = 1;

    if (g_world_shop_purchase_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_purchase_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF805, 0);
            g_world_menu_prompt_active = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_LIST;
        }
    } else {
        result = world_shop_run_item_purchase_quantity_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(-world_item_get_price(g_item_list_entries[g_menu_list_selected_index])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF811, 0);
            item_rebuild_category_list(g_world_shop_item_category);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_purchase_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode_variant(&g_world_shop_purchase_menu_script, 0, g_menu_input_lock_mode);
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C, 0, 1, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
}

/* Runs the shop purchase-quantity menu for the highlighted item.
 *
 * On entry, caps the selectable quantity at min(gil / price, 99 - owned) and
 * returns -2 when the stack is already full or -1 when no unit is
 * affordable. Afterwards it returns the quantity menu thread state, updating
 * the total-price prompt while the confirmation sub-thread is running. */
static s32 world_shop_run_item_purchase_quantity_menu(void) {
    s32 owned;
    s32 count;
    s32 limit;

    if (g_world_shop_quantity_menu_running == 0) {
        g_world_shop_purchase_total_shown = 0;
        g_world_menu_prompt_active = 1;
        g_world_shop_purchase_item = g_item_list_entries[g_menu_list_selected_index];
        owned = item_get_total_count_variant(g_world_shop_purchase_item);
        if (owned >= 99) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF807, 0);
            return WORLD_SHOP_QUANTITY_HOLDING_MAX;
        }
        count = world_shop_adjust_gil(0);
        g_world_shop_purchase_unit_price = world_item_get_price(g_world_shop_purchase_item);
        count /= g_world_shop_purchase_unit_price;
        limit = 99 - owned;
        if (limit < count) {
            count = limit;
        }
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF806, 0);
            return WORLD_SHOP_QUANTITY_CANNOT_AFFORD;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF808, 0);
        g_world_shop_quantity_limit = count;
        g_world_shop_purchase_total_shown = 0;
        g_sound_suppressed = 1;
    }
    g_world_shop_quantity_menu_running
        = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_quantity_menu_data);
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_world_shop_purchase_total_shown == 0) {
        g_text_substitution_values[0]
            = g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY] * g_world_shop_purchase_unit_price;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF809, 0);
        g_world_shop_purchase_total_shown = 1;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_world_shop_purchase_total_shown != 0) {
        g_world_shop_purchase_total_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF808, 0);
    }
    if (g_world_shop_quantity_menu_running == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_world_shop_quantity_menu_running;
}

/* Equip-menu marker for the selected item: 8 when the unit already wears it
 * or cannot equip it, -1 when it is a fresh equippable choice. */
static s32 world_item_get_equip_candidate_marker(s32 unit_index) {
    world_unit_t* unit = g_world_unit_pointers[unit_index];
    s32 i;

    for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
        if (unit->equipment[i] == g_world_shop_equip_candidate_item) {
            return MENU_UNIT_READOUT_EQUIP_LABEL;
        }
    }
    if (world_unit_can_equip_item_id(unit_index, g_world_shop_equip_candidate_item) != 1) {
        return MENU_UNIT_READOUT_EQUIP_LABEL;
    }
    return -1;
}

/* Menu callback: sprite brightness for a unit on the shop's "who can equip"
 * view: 0x80 when the unit can equip the candidate item, 0x40 when not. */
static s32 world_item_get_equip_candidate_brightness(s32 unit_index) {
    return world_unit_can_equip_item_id(unit_index, g_world_shop_equip_candidate_item) == 1 ? 0x80 : 0x40;
}

/*
 * Run one frame of the shop's "who can equip" unit browser for the item
 * under the list cursor.
 *
 * Moving onto a unit that can equip the item stages it in the temporary
 * formation preview record and shows the stat preview; slots
 * already holding the item are flagged in g_world_selected_unit_stat_detail.state.equipment. Cancel returns to shop
 * step 1.
 */
void world_shop_run_equip_candidate_step(void) {
    s32 unused[16];
    s32 input;
    u8 equipped;
    s32 i;
    s32 slot;
    s32 text_id;

    input = g_menu_input_secondary_repeat;
    if (g_world_shop_equip_candidate_initialized == 0) {
        menu_set_vsync_mode_and_event_speed(0);
        g_world_formation_unit_browse_enabled = 0;
        g_world_shop_background_visible = 0;
        g_world_shop_equip_candidate_item = g_item_list_entries[g_menu_list_selected_index];
        world_formation_init_cursor_and_scroll_state(0);
        g_world_shop_equip_candidate_preview_visible = 0;
        g_world_shop_equip_candidate_initialized = 1;
    }
    g_world_shop_equip_candidate_saved_browse = g_world_formation_unit_browse_enabled;
    world_formation_update_and_draw_unit_grid(0, g_world_formation_unit_browse_enabled == 0, 1, input,
        g_menu_input_lock_mode, world_item_get_equip_candidate_marker, world_item_get_equip_candidate_brightness);
    if (g_world_shop_equip_candidate_saved_browse == 0) {
        if (g_world_formation_unit_browse_enabled != 0) {
            g_world_shop_equip_candidate_last_unit = -1;
            return;
        }
    } else if (g_world_formation_unit_browse_enabled == 0) {
        g_menu_status_display_thread_params.redraw_request = 1;
    }
    if (g_world_formation_unit_banner_enabled != 0 && g_world_formation_unit_browse_enabled == 0) {
        text_id = MENU_GRID_HELP_BANNER_TOP;
        if (g_world_formation_unit_banner_at_bottom != 0) {
            text_id = MENU_GRID_HELP_BANNER_BOTTOM;
        }
        g_menu_help_message_text_id = text_id;
    } else {
        g_menu_help_message_text_id = 0;
    }
    if (g_world_formation_unit_browse_enabled != 0) {
        if (g_unit_selected_index != g_world_shop_equip_candidate_last_unit) {
            world_formation_stage_selected_unit();
            i = 0;
            equipped = 0;
            g_world_shop_equip_candidate_last_unit = g_unit_selected_index;
            for (; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                if (g_world_unit_pointers[g_unit_selected_index]->equipment[i] == g_world_shop_equip_candidate_item) {
                    equipped = 1;
                    g_world_selected_unit_stat_detail.state.equipment[i] |= 0x8000;
                }
            }
            if (equipped != 0
                || world_unit_can_equip_item_id(g_unit_selected_index, g_world_shop_equip_candidate_item) != 1) {
                g_world_shop_equip_candidate_preview_visible = 0;
            } else {
                slot = world_item_select_equipment_slot(
                    WORLD_FORMATION_PREVIEW_UNIT_INDEX, g_world_shop_equip_candidate_item);
                world_formation_equip_item_to_unit_slot(
                    WORLD_FORMATION_PREVIEW_UNIT_INDEX, slot, g_world_shop_equip_candidate_item);
                bcopy(g_world_unit_pointers[WORLD_FORMATION_PREVIEW_UNIT_INDEX]->equipment,
                    g_item_preview_stat_detail.state.equipment, 10);
                item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail,
                    &g_menu_selected_unit_stat_summary, g_world_unit_pointers[g_unit_selected_index]->equipment,
                    g_world_unit_pointers[WORLD_FORMATION_PREVIEW_UNIT_INDEX]->equipment);
                g_item_preview_stat_detail.state.two_hands = unit_is_two_handing_weapon(
                    g_item_preview_stat_detail.state.equipment, world_ability_has_two_hands(g_unit_selected_index));
                g_world_shop_equip_candidate_preview_visible = 1;
                g_item_preview_stat_detail.state.equipment[slot]
                    = g_world_shop_equip_candidate_item | WORLD_EQUIPMENT_SHOP_ITEM;
            }
        }
    } else {
        g_world_shop_equip_candidate_preview_visible = 0;
        if (g_world_shop_equip_candidate_saved_browse == 0 && (input & PSX_PAD_CROSS)) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_banner_enabled = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_BUY_LIST;
            menu_set_vsync_mode_and_event_speed(2);
            g_world_shop_background_visible = 1;
            g_world_shop_equip_candidate_initialized = 0;
        }
    }
    world_menu_toggle_preview_stats_window(g_world_shop_equip_candidate_preview_visible);
    world_menu_toggle_stat_preview_panel_thread(g_world_shop_equip_candidate_preview_visible);
}

/* Menu callback: half price of the highlighted item, or 0 for an empty list.
 * The row argument is not read. */
static s32 world_item_get_selected_half_price(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count == 0) {
        result = 0;
    } else {
        result = world_item_get_half_price(g_item_list_entries[g_menu_list_selected_index]);
    }
    return result;
}

/* Item-list shop menu step: on first entry prompts 0xF80B (0xF80C when the
 * player already holds items), rebuilds the item list and installs the seven
 * column callbacks, then handles cancel (0x40) and confirm (0x20).
 */
void world_shop_run_item_sell_list_step(void) {
    if (g_world_shop_sell_list_initialized == 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, world_menu_has_items(0) != 0 ? 0xF80C : 0xF80B, 0);
        g_world_shop_item_category = world_menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
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

/* Item sell confirmation step: pays half price times the quantity, removes the
 * items, prompts 0xF811 and rebuilds the list (0xF80A when nothing to sell).
 */
void world_shop_run_item_sell_confirm_step(void) {
    s32 result = 1;

    if (g_world_shop_sell_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_sell_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80D, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_SELL_LIST;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_item_sell_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(world_item_get_half_price(g_item_list_entries[g_menu_list_selected_index])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF811, 0);
            item_rebuild_category_list(g_world_shop_item_category);
            world_item_build_category_list(0, g_world_shop_item_category + 1,
                g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            world_menu_init_and_load_scrollable_list(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_world_shop_sell_list_menu_script);
            menu_set_window_scale_step(10);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_sell_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_sell_list_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    world_shop_run_obtain_gil_menu();
}

/* Shop: the quantity prompt of a sale, capped by the inventory count.
 *
 * The first call returns -1 with message 0xF80E when none of the highlighted
 * item is in the inventory. Otherwise it returns the quantity menu's thread
 * state, showing the total (half price times quantity, message 0xF810) while
 * the confirmation child runs and the plain prompt (0xF80F) otherwise. The
 * final frame queues the transaction sound for "yes" and the confirm sound
 * for "no". */
static s32 world_shop_run_item_sell_menu(void) {
    s32 count;

    if (g_world_shop_sell_result == 0) {
        g_world_shop_sell_confirm_shown = 0;
        g_world_shop_sell_item = g_item_list_entries[g_menu_list_selected_index];
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

/* Menu callback: add `delta` to the fitting room's running cost and return
 * the new total. */
static s32 world_shop_add_fitting_room_cost(s32 delta) {
    return g_world_shop_fitting_room_cost = delta + g_world_shop_fitting_room_cost;
}

/* Menu callback: no marker (-1) for any unit. */
static s32 world_shop_fitting_room_get_unit_marker(s32 unused_unit_index) {
    return -1;
}

/* Fitting-room menu step: on entry clear the trial equipment and its cost;
 * then either browse the formation (confirm opens the fitting menu, cancel
 * returns to shop step 0xC) or run the fitting menu thread and dispatch its
 * selection to the next shop step. */
void world_shop_run_fitting_room_step(void) {
    s32 busy;

    if (g_world_shop_fitting_room_initialized == 0) {
        if (g_menu_input_newly_pressed == 0) {
            world_shop_run_obtain_gil_menu();
            return;
        }
        world_formation_init_cursor_and_scroll_state(0);
        memset(g_world_shop_fitting_room_items, 0, sizeof(g_world_shop_fitting_room_items));
        world_shop_add_fitting_room_cost(-world_shop_add_fitting_room_cost(0));
        g_world_shop_fitting_room_unit_chosen = 0;
        g_world_shop_cost_window_visible = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_world_shop_fitting_room_initialized = 1;
        return;
    }
    if (g_world_shop_fitting_room_unit_chosen == 0) {
        g_world_shop_background_visible = 0;
        if (g_world_formation_unit_banner_enabled != 0) {
            g_menu_help_message_text_id = (g_world_formation_unit_banner_at_bottom != 0) ? MENU_GRID_HELP_BANNER_BOTTOM
                                                                                         : MENU_GRID_HELP_BANNER_TOP;
        } else {
            g_menu_help_message_text_id = 0;
        }
        busy = world_input_get_lockout_timer();
        busy += world_formation_is_unit_list_scroll_needed();
        if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) && busy == 0) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_banner_enabled = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_CHECKOUT;
        } else if ((g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE)) && busy == 0) {
            g_world_formation_unit_cycle_mode = 2;
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_world_shop_fitting_room_unit_chosen = 1;
            g_world_formation_unit_browse_enabled = 1;
            g_world_formation_unit_banner_at_bottom = 0;
            g_world_shop_cost_window_visible = 1;
            g_world_shop_fitting_room_menu_thread_data[0].selected_index = 0;
            g_world_shop_fitting_room_menu_thread_data[1].selected_index = 0;
            menu_clear_selection_records();
        } else {
            world_formation_update_and_draw_unit_grid(1, 1, 0, g_menu_input_secondary_repeat, g_menu_input_lock_mode,
                world_shop_fitting_room_get_unit_marker, 0);
        }
    } else {
        g_world_shop_background_visible = 1;
        g_menu_help_message_text_id = 0;
        if (menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_world_shop_fitting_room_menu_thread_data) == 0) {
            if (g_world_menu_selection_results[1] == 0) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_EQUIP;
            } else if (g_world_menu_selection_results[1] == 1) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_BEST_FIT;
            } else if (g_world_menu_selection_results[1] == 2) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_REMOVE;
            } else if (g_world_menu_selection_results[1] == 3) {
                if (g_world_menu_selection_results[2] == 0) {
                    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_SELL_EQUIPPED;
                } else {
                    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_SELL;
                }
            } else {
                g_world_shop_fitting_room_unit_chosen = 0;
                world_formation_init_selection_cursor(g_unit_selected_index);
                g_world_formation_unit_cycle_mode = 0;
                g_world_formation_unit_browse_enabled = 0;
                g_world_shop_cost_window_visible = 0;
                if (g_world_menu_selection_results[1] == 4) {
                    g_world_formation_unit_banner_enabled = 0;
                    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_CHECKOUT;
                }
            }
            menu_init_selection_value(WORLD_MENU_CURSOR_REMOVE_SLOT, 0);
            menu_init_selection_value(WORLD_MENU_CURSOR_EQUIP_SLOT, 0);
        }
    }
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}

/* Shop fitting-room equipment step, the shop twin of world_formation_run_equip_item_menu.
 *
 * With the slot list active, steps the equipment-slot cursor and shows the
 * equipped item's description; confirming opens the shop item list for the
 * slot. In the item list, confirming an equippable item returns the unit's
 * current item to the fitting room, equips the shop item (bit 15 marks it as
 * unpaid), adds its price to the fitting-room cost and recalculates stats,
 * while moving the cursor previews the stat change. Cancel leaves the list,
 * or the step (g_world_shop_menu_step = 6) from the slot list. */
void world_shop_run_fitting_room_equip_menu(void) {
    s16 x;
    s16 y;
    s32 prev;
    s32 slot;
    s32 value;
    u16 item;
    s16 equip;
    s32 result;
    s16* cursor_y;

    if (g_world_shop_fitting_equip_menu_initialized == 0) {
        g_world_shop_fitting_equip_menu_initialized = 1;
        g_world_shop_fitting_room_previewed_cursor = -1;
        g_world_shop_fitting_room_item_list_open = 0;
        g_menu_list_selected_index = 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        if (g_world_shop_fitting_room_item_list_open != 0) {
            g_world_shop_fitting_item_list_close = 1;
        } else {
            g_world_shop_fitting_equip_menu_initialized = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
        }
    } else if (g_world_shop_fitting_room_item_list_open == 0) {
        prev = g_world_shop_fitting_equip_slot;
        g_world_shop_fitting_equip_slot = menu_update_vertical_selection_and_mark_change(WORLD_EQUIPMENT_SLOT_COUNT,
            WORLD_MENU_CURSOR_EQUIP_SLOT, g_menu_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
        if (prev != g_world_shop_fitting_equip_slot) {
            g_menu_list_selected_index = 0;
        }
        if (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_fitting_equip_slot]
            != ITEM_ID_NOTHING) {
            value = (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_fitting_equip_slot]
                        & WORLD_ITEM_ID_MASK)
                + 0x6800;
        } else {
            value = -1;
        }
        g_menu_help_message_text_id = value;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_world_formation_unit_cycle_mode = 1;
            g_world_shop_fitting_room_item_list_open = 1;
            g_world_shop_fitting_room_previewed_cursor = -1;
            g_world_shop_cost_window_visible = 0;
            g_world_change_banner_panel_thread_params.style = 1;
            world_shop_build_item_list(g_unit_selected_index, g_world_menu_context_id, g_world_shop_fitting_equip_slot,
                g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);
            slot = 0;
            if (g_world_shop_fitting_equip_slot != 0) {
                slot = g_world_shop_fitting_equip_slot - 1;
            }
            menu_get_selection_record_variant(slot, &x, &y, g_item_list_entries);
            menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
            g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
            g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
            g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
            g_world_menu_script_callbacks[4].value = world_menu_get_item_price_display_value;
            menu_set_vsync_mode_and_event_speed(2);
        }
    } else {
        item = g_item_list_entries[g_menu_list_selected_index];
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0 || (item & WORLD_ITEM_LIST_ENTRY_UNEQUIPPABLE)) {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                equip = item;
                result = world_formation_can_equip_item_in_slot(
                    g_unit_selected_index, g_world_shop_fitting_equip_slot, equip);
                if (result < 0) {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                } else {
                    world_shop_return_unit_equip_to_fitting_room(
                        g_unit_selected_index, g_world_shop_fitting_equip_slot);
                    g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_fitting_equip_slot]
                        = item | WORLD_EQUIPMENT_SHOP_ITEM;
                    world_shop_add_fitting_room_cost(world_item_get_price(equip));
                    if (result != 1) {
                        world_shop_return_unit_equip_to_fitting_room(
                            g_unit_selected_index, g_world_shop_fitting_equip_slot == 0);
                    }
                    g_world_shop_fitting_item_list_close = 1;
                    g_sound_queued_effect_id = MAIN_SFX_EQUIP;
                    world_formation_recalculate_selected_unit_stats();
                }
            }
        }
        if (g_menu_list_entry_count != 0 && g_world_shop_fitting_room_previewed_cursor != g_menu_list_selected_index) {
            g_world_shop_fitting_room_previewed_cursor = g_menu_list_selected_index;
            result = world_formation_equip_item_to_unit_slot(
                WORLD_FORMATION_PREVIEW_UNIT_INDEX, g_world_shop_fitting_equip_slot, item);
            if (result == 1) {
                world_menu_toggle_preview_stats_window(1);
                g_menu_preview_stats_thread_params.redraw_request = result;
                item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail,
                    &g_menu_selected_unit_stat_summary, g_world_unit_pointers[g_unit_selected_index]->equipment,
                    g_world_formation_temp_unit->equipment);
                bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, g_world_formation_temp_unit->equipment,
                    10);
            } else {
                world_menu_toggle_preview_stats_window(0);
            }
            g_menu_status_display_thread_params.redraw_request = 1;
        }
        menu_run_script_with_palette_mode_variant(
            &g_world_shop_fitting_item_list_menu_script, g_menu_input_primary_repeat, world_map_is_busy());
        g_menu_help_message_text_id = g_menu_list_entry_count != 0
            ? (g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_ID_MASK) + 0x6800
            : -1;
    }
    if (g_world_shop_fitting_item_list_close != 0) {
        g_world_shop_cost_window_visible = 1;
        g_world_shop_fitting_item_list_close = 0;
        g_world_change_banner_panel_thread_params.style = 0;
        g_world_shop_fitting_room_item_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        menu_set_vsync_mode_and_event_speed(0);
        world_menu_toggle_preview_stats_window(0);
        g_menu_status_display_thread_params.redraw_request = 1;
        slot = 0;
        if (g_world_shop_fitting_equip_slot != 0) {
            slot = g_world_shop_fitting_equip_slot - 1;
        }
        menu_set_selection_record(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
    }
    cursor_y = &g_world_shop_fitting_slot_cursor_point.y;
    *cursor_y = g_world_shop_fitting_equip_slot * 16 + 0x90;
    gfx_set_otag_index(0x3d);
    menu_update_and_draw_sliding_cursor_variant(&g_world_shop_fitting_slot_cursor_point,
        &g_world_shop_fitting_slot_cursor_anim,
        g_world_shop_fitting_room_item_list_open != 0 || g_menu_input_lock_mode != 0);
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}

static s32 world_shop_get_gil_minus_fitting_room_cost(s32 unused_row) {
    s32 gil;

    gil = world_shop_adjust_gil(0);
    return gil - world_shop_add_fitting_room_cost(0);
}

/* Report whether a menu task or thread 6 is still running.
 *
 * Its only caller passes 0, but the body never reads that argument: `$a0` is
 * overwritten with 6 before any use and the result starts from zero. */
static s32 world_menu_is_busy(s32 unused) {
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
static void world_shop_install_callbacks_and_run(void) {
    g_world_menu_script_callbacks[0].value = world_shop_adjust_gil;
    g_world_menu_script_callbacks[1].value = world_shop_add_fitting_room_cost;
    g_world_menu_script_callbacks[2].value = world_shop_get_gil_minus_fitting_room_cost;
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_fitting_cost_menu_script, g_menu_input_primary_repeat, world_menu_is_busy(0));
}

/* Fitting-room counterpart of world_menu_run_remove_item: takes one equipment
 * slot off the selected unit, refunding a shop item's price from the fitting
 * room cost, and previews the stat change of the slot under the cursor
 * against the unit's own item held in the fitting room.
 *
 * The prologue's $a0 is not an argument register here. The callee at
 * 0x80114d4c is world_shop_update_unit_selection, which takes no
 * arguments; $a0 simply holds the copy of the selected-unit index that the
 * store to g_world_shop_previewed_unit_index consumes, while the compare keeps the load in $v1.
 * A one-argument prototype would pull the load pseudo into $a0 and push the
 * copy into $v1; pins and launders cannot compensate, because the target's
 * copy sits in the beq's delay slot and a tied asm is never a delay-slot
 * candidate. */
void world_shop_run_fitting_room_remove_step(void) {
    /* The target reserves 64 local bytes whose contents are not accessed;
     * saved registers sit at 0x60..0x70 under a -0x78 frame. */
    s32 unused[16];
    s8 slot;
    s32 index;
    s32 value;
    s32 equipped_item;
    s16* cursor_y;
    s16 unit;

    if (g_world_shop_previewed_unit_index != g_unit_selected_index) {
        unit = g_unit_selected_index;
        g_world_shop_previewed_unit_index = unit;
        g_world_shop_equip_slot_preview_initialized = 0;
        world_shop_update_unit_selection();
        menu_input_clear_state();
    }
    if (g_world_shop_equip_slot_preview_initialized == 0) {
        g_world_shop_previewed_equip_slot = -1;
        g_world_shop_equip_slot_preview_initialized = 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        world_menu_toggle_preview_stats_window(0);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
        g_world_shop_equip_slot_preview_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
    }
    slot = menu_update_vertical_selection_and_mark_change(
        WORLD_EQUIPMENT_SLOT_COUNT, WORLD_MENU_CURSOR_REMOVE_SLOT, g_menu_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
    if (g_world_unit_pointers[g_unit_selected_index]->equipment[(u8)slot] != ITEM_ID_NOTHING) {
        value = (g_world_unit_pointers[g_unit_selected_index]->equipment[(u8)slot] & WORLD_ITEM_ID_MASK) + 0x6800;
    } else {
        value = -1;
    }
    g_menu_help_message_text_id = value;
    cursor_y = &g_world_shop_equip_slot_cursor_point.y;
    *cursor_y = (u8)slot * 16 + 0x90;
    gfx_set_otag_index(0x3D);
    menu_update_and_draw_sliding_cursor_variant(
        &g_world_shop_equip_slot_cursor_point, &g_world_shop_equip_slot_cursor_anim, world_thread_is_task_active(0));
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        equipped_item = g_world_unit_pointers[g_unit_selected_index]->equipment[(u8)slot];
        if ((equipped_item >> 15) == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            world_shop_add_fitting_room_cost(-world_item_get_price(equipped_item & WORLD_ITEM_ID_MASK));
            world_shop_finalize_unit_equips_from_fitting_room(g_unit_selected_index, slot & 0xFF);
            world_formation_recalculate_selected_unit_stats();
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
            g_world_shop_previewed_equip_slot = -1;
        }
    }
    index = (u8)slot;
    if (g_world_shop_previewed_equip_slot != index) {
        equipped_item = g_world_unit_pointers[g_unit_selected_index]->equipment[index];
        g_world_shop_previewed_equip_slot = slot;
        if ((equipped_item >> 15) != 0) {
            world_menu_toggle_preview_stats_window(1);
            menu_calculate_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
                equipped_item, g_world_shop_fitting_room_items[g_unit_selected_index][index], index);
            g_menu_preview_stats_thread_params.redraw_request = 1;
        } else {
            world_menu_toggle_preview_stats_window(0);
        }
        g_menu_status_display_thread_params.redraw_request = 1;
    }
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}

/*
 * Fitting room step that prices the tried-on equipment.
 *
 * Runs the checkout prompt: done returns to the shop's main menu (message
 * 0xf802); canceled shows message 0xf818 until any face button, which
 * returns to the fitting room with the unit's stats recalculated.
 */
void world_shop_run_fitting_room_checkout_step(void) {
    s32 result;

    g_world_shop_background_visible = 1;
    if (g_world_shop_checkout_complete != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_world_shop_checkout_complete = 0;
            g_world_formation_unit_banner_enabled = 1;
            world_formation_recalculate_selected_unit_stats();
        }
    } else {
        result = world_shop_run_fitting_room_checkout_prompt();
        if (result == WORLD_SHOP_CHECKOUT_DONE) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_MAIN_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf802, 0);
            g_world_shop_fitting_room_initialized = 0;
        } else if (result == WORLD_SHOP_CHECKOUT_CANCELED) {
            g_world_shop_checkout_complete = 1;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf818, 0);
        }
    }
    world_shop_run_obtain_gil_menu();
}

/* Pay and cannot-afford prompt menus; their cursors are reset on entry. */

/*
 * Run one frame of the fitting-room checkout prompt.
 *
 * Opens the pay prompt when the party can afford the fitting-room cost and
 * the "not enough gil" prompt otherwise; an empty cart is bought
 * immediately. Returns 1 while a prompt is open; 0 and -1 are the two
 * closing outcomes world_shop_run_fitting_room_checkout_step dispatches on.
 */
static s32 world_shop_run_fitting_room_checkout_prompt(void) {
    s32 balance;
    s32 cost;

    if (g_world_shop_checkout_prompt_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
            return WORLD_SHOP_CHECKOUT_PROMPT_OPEN;
        }
        g_world_shop_checkout_confirm_menu.selected_index = 0;
        g_world_shop_checkout_short_gil_menu.selected_index = 0;
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        balance = world_shop_adjust_gil(0) - (cost = world_shop_add_fitting_room_cost(0));
        if (cost == 0) {
            world_shop_buy_from_fitting_room();
            return WORLD_SHOP_CHECKOUT_DONE;
        }
        if (balance >= 0) {
            g_sound_suppressed = 1;
            g_world_shop_checkout_prompt_menu = &g_world_shop_checkout_confirm_menu;
            g_text_substitution_values[0] = cost;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf819, 0);
            g_world_shop_checkout_insufficient_gil = 0;
        } else {
            g_world_shop_checkout_prompt_menu = &g_world_shop_checkout_short_gil_menu;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf81a, 0);
            g_world_shop_checkout_insufficient_gil = 1;
        }
        g_world_shop_checkout_prompt_initialized = 1;
        g_world_shop_checkout_prompt_active = 1;
    }
    if (g_world_shop_checkout_prompt_active != 0) {
        g_world_shop_checkout_prompt_active
            = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_checkout_prompt_menu);
        if (g_world_shop_checkout_prompt_active != 0) {
            return WORLD_SHOP_CHECKOUT_PROMPT_OPEN;
        }
        g_world_shop_checkout_prompt_result = g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT];
        if (g_world_shop_checkout_insufficient_gil == 0) {
            g_sound_suppressed = 0;
            if (g_world_shop_checkout_prompt_result == 0) {
                balance = -world_shop_add_fitting_room_cost(0);
                world_shop_adjust_gil(balance);
                world_shop_add_fitting_room_cost(balance);
                world_shop_buy_from_fitting_room();
                world_formation_save_records_to_party();
                g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
                item_rebuild_category_list(0);
                item_rebuild_category_list(1);
                item_rebuild_category_list(2);
                item_rebuild_category_list(3);
                item_rebuild_category_list(4);
                g_world_shop_checkout_prompt_initialized = 0;
                return WORLD_SHOP_CHECKOUT_DONE;
            }
            if (g_world_shop_checkout_prompt_result == 1) {
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            }
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf81c, 0);
            return WORLD_SHOP_CHECKOUT_PROMPT_OPEN;
        }
        if (g_world_shop_checkout_prompt_result != 1) {
            world_shop_apply_fitting_room_items_to_all_units();
            g_world_shop_checkout_prompt_initialized = 0;
            return WORLD_SHOP_CHECKOUT_DONE;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_world_shop_checkout_prompt_initialized = 0;
        return WORLD_SHOP_CHECKOUT_CANCELED;
    }
    if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_shop_checkout_short_gil_menu) != 0) {
        return WORLD_SHOP_CHECKOUT_PROMPT_OPEN;
    }
    g_world_shop_checkout_prompt_initialized = 0;
    if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] != 1) {
        if (g_world_shop_checkout_prompt_result != 0) {
            world_shop_apply_fitting_room_items_to_all_units();
            return WORLD_SHOP_CHECKOUT_DONE;
        }
        return WORLD_SHOP_CHECKOUT_DONE;
    }
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
    return WORLD_SHOP_CHECKOUT_CANCELED;
}

/* Move every reserved fitting-room item onto its formation unit and clear
 * the reservation. Equipment slots flagged in bit 14 are overwritten even
 * when no item was reserved. */
static void world_shop_apply_fitting_room_items_to_all_units(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_world_formation_unit_count; i++) {
        for (j = 0; j < WORLD_EQUIPMENT_SLOT_COUNT; j++) {
            if ((g_world_unit_pointers[i]->equipment[j] >> 14) != 0
                || g_world_shop_fitting_room_items[i][j] != ITEM_ID_NOTHING) {
                g_world_unit_pointers[i]->equipment[j] = g_world_shop_fitting_room_items[i][j];
            }
            g_world_shop_fitting_room_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}

/*
 * Shop "best fit / fitting room" preview step, entry 8 of the shop callback
 * table at 0x8019525c.
 *
 * On entry it stages the selected unit's four weapon/armour slots, fills any
 * slot already taken from the shop with that slot's fitting-room reservation,
 * asks 0x80123dd8 for a suggested set, restores the staged slots and previews
 * the resulting stat delta. It then runs the confirmation menu; on confirm it
 * commits the suggested set slot by slot, returning each displaced item to the
 * fitting room or refunding a shop item's price, and returns to shop step 6.
 *
 * The gate at 0x801957a8 is `s8`, not `u8` (the target sign-loads it, `lb`).
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
 * The target has `move s1,v0` after world_menu_run_thread and no
 * initialization of s1 at loop 2's head: loop 2's counter is the run_thread
 * result's allocno, so the source assigns the result to `i` and the loop is
 * written `for (; i < WORLD_EQUIPMENT_SLOT_ACCESSORY; i++)`. A fresh block-local takes a call-clobbered
 * register; reusing a function-scope allocno that is already dead puts the
 * counter in the target's callee-saved s1 and decides which of the two loop
 * quantities is allocated first.
 *
 * In the loop-1 preheader the target computes
 * `&g_world_unit_pointers[idx]` first (into a1) and the
 * fitting-room row base second (into a0). Giving the unit slot its own local
 * (`unit_slot`), assigned just before `reserved`, and reading the loop body
 * through `(*unit_slot)->equipment[i]`, puts the two preheader address
 * computations in that order. The `lw` stays inside the loop, since the store
 * through `reserved` may alias the pointer array.
 *
 * Within `reserved`'s address expression the target materialises the
 * fitting-room base `lui`/`addiu` before the `idx*5*2` index arithmetic. cc1
 * itself emits `addu`/`sll`/`la`/`addu` in the other order under one `.loc`,
 * because the symbol canonicalises to the second operand of the address PLUS
 * and is expanded last; no operand respelling changes that (`fold` rewrites
 * `base - -(idx * 5)` straight back to a PLUS). Giving the row base its own
 * local and its own statement (`room_base = &g_world_shop_fitting_room_items[0][0];`
 * then `reserved = room_base + idx * 5;`) forces the `la` to be emitted at
 * that statement, ahead of the index arithmetic.
 *
 * The confirmation result g_world_menu_selection_results[5] is read directly
 * each time. Its address stays in one register across the calls and the last
 * read is scheduled ahead of the two byte stores that precede it in source.
 */
void world_shop_run_best_fit_preview_step(void) {
    /* The target reserves vars the code never touches: args 16 + vars 88 +
     * 6 saves 24 = the frame of 128, while only the 8-byte staging buffer at
     * sp+80 is accessed. */
    s32 unused[16];
    u16 staged[4];
    s32 unused_tail[2];
    s32 i;
    s32 item;

    if (g_world_shop_best_fit_menu_open == 0) {
        world_unit_t** unit_slot;
        s16* room_base;
        s16* reserved;

        g_world_shop_best_fit_menu.selected_index = 0;
        bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, staged, sizeof(staged));
        unit_slot = &g_world_unit_pointers[g_unit_selected_index];
        room_base = &g_world_shop_fitting_room_items[0][0];
        reserved = room_base + g_unit_selected_index * WORLD_EQUIPMENT_SLOT_COUNT;
        for (i = 0; i < WORLD_EQUIPMENT_SLOT_ACCESSORY; i++, reserved++) {
            if (((*unit_slot)->equipment[i] >> 15) != 0) {
                (*unit_slot)->equipment[i] = *reserved;
            }
        }
        world_shop_build_fitting_room_best_fit_equipment(
            g_unit_selected_index, g_world_menu_context_id, g_item_preview_stat_detail.state.equipment);
        g_item_preview_stat_detail.state.equipment[WORLD_EQUIPMENT_SLOT_ACCESSORY]
            = g_world_unit_pointers[g_unit_selected_index]->equipment[WORLD_EQUIPMENT_SLOT_ACCESSORY];
        bcopy(staged, g_world_unit_pointers[g_unit_selected_index]->equipment, sizeof(staged));
        item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
            g_world_unit_pointers[g_unit_selected_index]->equipment, (u16*)g_item_preview_stat_detail.state.equipment);
        g_item_preview_stat_detail.state.two_hands = unit_is_two_handing_weapon(
            g_item_preview_stat_detail.state.equipment, world_ability_has_two_hands(g_unit_selected_index));
        world_menu_toggle_stat_preview_panel_thread(1);
        world_menu_toggle_preview_stats_window(1);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_menu_preview_stats_thread_params.redraw_request = 1;
        g_world_shop_best_fit_menu_open = 1;
        g_world_formation_unit_cycle_mode = 1;
        g_sound_suppressed = 1;
    }
    i = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_shop_best_fit_menu);
    if (i == 0) {
        if (g_world_menu_selection_results[5] == 0) {
            g_sound_queued_effect_id = MAIN_SFX_EQUIP;
        }
        if (g_world_menu_selection_results[5] == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        world_menu_toggle_stat_preview_panel_thread(0);
        world_menu_toggle_preview_stats_window(0);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_world_shop_best_fit_menu_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        if (g_world_menu_selection_results[5] == 0) {
            for (; i < WORLD_EQUIPMENT_SLOT_ACCESSORY; i++) {
                u16 suggested;
                s32 shifted;
                s32 current;

                /* The target reads the suggested item unsigned and widens it
                 * explicitly, reusing the shifted value for the sign test
                 * (`lhu` / `sll 16` / `sra 16`, then `bgez` on the shift), and
                 * it loads the unit's own slot ONCE at the loop head -- the
                 * `sra ,0xf` lands in the `bgez` delay slot and the same
                 * register is sign-extended again for world_item_get_price. */
                suggested = g_item_preview_stat_detail.state.equipment[i];
                shifted = suggested << 16;
                item = shifted >> 16;
                current = g_world_unit_pointers[g_unit_selected_index]->equipment[i];
                if (item == 0 || shifted < 0) {
                    world_shop_return_unit_equip_to_fitting_room(g_unit_selected_index, i);
                    world_shop_add_fitting_room_cost(world_item_get_price(item));
                } else if ((current >> 15) != 0) {
                    world_shop_add_fitting_room_cost(-world_item_get_price(current));
                    g_world_shop_fitting_room_items[g_unit_selected_index][i] = ITEM_ID_NOTHING;
                }
                g_world_unit_pointers[g_unit_selected_index]->equipment[i] = item;
            }
            world_formation_recalculate_selected_unit_stats();
        }
        g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
        g_sound_suppressed = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
    }
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}

/*
 * Run one frame of the shop's sell-equipped-item menu.
 *
 * The cursor walks the selected unit's five equipment slots; confirming a
 * slot the unit owns (non-empty, not taken from the shop) opens a sale prompt
 * for half the item's price and, on acceptance, credits the gil and clears
 * the slot. Triangle returns to shop step 6.
 */
void world_shop_run_sell_equipped_item_menu(void) {
    s32 close;
    u32 item;
    s32 text_id;
    u32 input;

    close = 0;
    if (g_world_shop_sell_equipped_menu_open == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80c, 0);
            g_world_shop_sell_equipped_prompt_active = 0;
            g_world_formation_unit_cycle_mode = 1;
            g_world_shop_sell_equipped_menu_open = 1;
            g_world_shop_sell_equipped_result_shown = 0;
        }
    } else {
        if (g_world_shop_sell_equipped_result_shown != 0) {
            if (g_menu_input_primary_repeat
                & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80d, 0);
                g_world_shop_sell_equipped_result_shown = 0;
            }
        } else if (g_world_shop_sell_equipped_prompt_active == 0) {
            g_world_shop_sell_equipped_slot = menu_update_vertical_selection_and_mark_change(WORLD_EQUIPMENT_SLOT_COUNT,
                WORLD_MENU_CURSOR_REMOVE_SLOT, g_menu_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
            if (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot]
                != ITEM_ID_NOTHING) {
                text_id = (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot]
                              & WORLD_ITEM_ID_MASK)
                    + 0x6800;
            } else {
                text_id = -1;
            }
            input = g_menu_input_primary_repeat;
            g_menu_help_message_text_id = text_id;
            if (input & PSX_PAD_CIRCLE) {
                item = g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot];
                if (item == ITEM_ID_NOTHING || (item >> 15) != 0) {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                } else {
                    g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                    g_world_shop_sell_equipped_prompt_active = 1;
                    g_world_shop_sell_equipped_menu.selected_index = 0;
                    g_text_substitution_values[0] = world_item_get_half_price(item);
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf810, 0);
                }
            } else if (input & PSX_PAD_CROSS) {
                close = 1;
            }
        } else {
            g_menu_help_message_text_id = 0;
            g_sound_suppressed = g_world_shop_sell_equipped_prompt_active
                = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_shop_sell_equipped_menu);
            if (g_world_shop_sell_equipped_prompt_active == 0) {
                if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf811, 0);
                    world_shop_adjust_gil(g_text_substitution_values[0]);
                    g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_sell_equipped_slot]
                        = ITEM_ID_NOTHING;
                    world_formation_recalculate_selected_unit_stats();
                    g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
                } else {
                    if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
                        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                    }
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80a, 0);
                }
                g_world_shop_sell_equipped_result_shown = 1;
            }
        }
        if (close != 0) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_cycle_mode = 2;
            g_world_shop_sell_equipped_menu_open = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        g_world_shop_sell_equipped_cursor_point.y = g_world_shop_sell_equipped_slot * 16 + 0x90;
        gfx_set_otag_index(0x3d);
        menu_update_and_draw_sliding_cursor_variant(&g_world_shop_sell_equipped_cursor_point,
            &g_world_shop_sell_equipped_cursor_anim, world_thread_is_task_active(0));
    }
    world_shop_install_callbacks_and_run();
}

/*
 * Run the menu's state-4/state-5 subflow through thread 2.
 *
 * State 0 closes the subflow and hands control back as state 6; otherwise
 * the updated state is retained for the next call. The menu role is unverified.
 */
void world_shop_run_sell_menu_subflow_step(void) {
    if (g_world_shop_sell_subflow_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
            return;
        }
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        g_world_formation_unit_browse_enabled = 0;
        g_world_formation_unit_banner_enabled = 0;
        world_formation_stop_menu_threads();
        g_world_formation_unit_cycle_mode = 0;
        g_world_shop_sell_subflow_step = WORLD_SHOP_STEP_SELL_LIST;
        menu_clear_selection_record(5);
        menu_clear_selection_record(6);
        menu_clear_selection_record(7);
        menu_clear_selection_record(8);
        menu_clear_selection_record(9);
        g_world_shop_sell_subflow_initialized = 1;
    }
    g_world_shop_menu_step = g_world_shop_sell_subflow_step;
    if (g_world_shop_sell_subflow_step == WORLD_SHOP_STEP_SELL_LIST) {
        world_shop_run_item_sell_list_step();
        if (g_world_shop_sell_subflow_step != g_world_shop_menu_step) {
            world_menu_reset_selection_results();
        }
    } else if (g_world_shop_sell_subflow_step == WORLD_SHOP_STEP_SELL_CONFIRM) {
        world_shop_run_item_sell_confirm_step();
    }
    if (g_world_shop_menu_step == WORLD_SHOP_STEP_MAIN_MENU) {
        g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
        g_world_formation_unit_cycle_mode = 2;
        g_world_shop_sell_subflow_initialized = 0;
        g_world_formation_unit_browse_enabled = 1;
        menu_set_vsync_mode_and_event_speed(2);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        return;
    }
    g_world_shop_sell_subflow_step = g_world_shop_menu_step;
    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_SELL;
}

/* Clears one equipment slot of a formation unit.  An item the unit owned goes
 * back to the fitting room reserve; an item taken from the shop (bit 15 set)
 * is refunded from the fitting room cost instead. */
static void world_shop_return_unit_equip_to_fitting_room(s16 unit_index, s16 equipment_index) {
    s32 equipped_item = g_world_unit_pointers[unit_index]->equipment[equipment_index];

    if (equipped_item != ITEM_ID_NOTHING) {
        if ((equipped_item >> 15) == 0)
            g_world_shop_fitting_room_items[unit_index][equipment_index] = equipped_item;
        else
            world_shop_add_fitting_room_cost(-world_item_get_price(equipped_item & WORLD_ITEM_ID_MASK));
        g_world_unit_pointers[unit_index]->equipment[equipment_index] = ITEM_ID_NOTHING;
    }
}

/* Finalize a unit's equipment from the Fitting Room. */
static void world_shop_finalize_unit_equips_from_fitting_room(s16 formation_index, s16 equipment_index) {
    u16 reserved_item;

    g_world_unit_pointers[formation_index]->equipment[equipment_index] = ITEM_ID_NOTHING;
    reserved_item = g_world_shop_fitting_room_items[formation_index][equipment_index];
    if (equipment_index < WORLD_EQUIPMENT_SLOT_HEAD) {
        u16 right_hand = g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
        u16 left_hand = g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND];

        if (right_hand == ITEM_ID_NOTHING && left_hand == ITEM_ID_NOTHING) {
            g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND]
                = g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
            g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND]
                = g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_LEFT_HAND];
            g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_RIGHT_HAND] = ITEM_ID_NOTHING;
            g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
            return;
        }
        if (reserved_item != ITEM_ID_NOTHING
            && world_formation_can_equip_item_in_slot(g_unit_selected_index, equipment_index, reserved_item) == 1) {
            g_world_unit_pointers[formation_index]->equipment[equipment_index] = reserved_item;
            g_world_shop_fitting_room_items[formation_index][equipment_index] = ITEM_ID_NOTHING;
        }
    } else {
        g_world_unit_pointers[formation_index]->equipment[equipment_index] = reserved_item;
        g_world_shop_fitting_room_items[formation_index][equipment_index] = ITEM_ID_NOTHING;
    }
}

/* Complete a fitting-room purchase: every tried-on item (entry bits 14-15
 * set) becomes the unit's own, and the equipment it displaced, kept in
 * g_world_shop_fitting_room_items, goes back to the inventory. */
static void world_shop_buy_from_fitting_room(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_world_formation_unit_count; i++) {
        for (j = 0; j < WORLD_EQUIPMENT_SLOT_COUNT; j++) {
            s32 equipped_item = g_world_unit_pointers[i]->equipment[j];
            if ((equipped_item >> 14) != 0) {
                g_world_unit_pointers[i]->equipment[j] &= WORLD_ITEM_ID_MASK;
            }
            item_adjust_inventory_count(g_world_shop_fitting_room_items[i][j], 1);
            g_world_shop_fitting_room_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}

void world_shop_run_fur_menu_step(void) {
    s32 i;

    if (g_world_shop_fur_opened == 0) {
        g_menu_list_selected_index = 0;
        menu_set_vsync_mode_and_event_speed(0);
        g_world_shop_fur_has_secret_hunt = 0;
        g_world_shop_fur_has_poached_stock = 0;
        g_world_shop_fur_access_granted = 0;
        g_world_shop_fur_waiting_message = 0;
        for (i = 0; i < g_world_formation_unit_count; i++) {
            if (world_ability_has_secret_hunt(i) != 0) {
                g_world_shop_fur_has_secret_hunt = 1;
            }
        }
        if (g_world_shop_fur_has_secret_hunt != 0) {
            for (i = 1; i < 0x100; i++) {
                if (world_shop_adjust_poached_item_quantity(i, 0) != 0) {
                    g_world_shop_fur_has_poached_stock = 1;
                }
            }
        }
        if (g_world_shop_fur_has_secret_hunt == 0) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82D, 0);
            return;
        }
        g_world_shop_fur_access_granted = 1;
        g_world_shop_fur_opened = 1;
    }
    if (g_world_shop_fur_waiting_message != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF830, 0);
            g_world_shop_fur_waiting_message = 0;
        }
    } else if (menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_shop_fur_menu_thread_data) == 0) {
        menu_clear_selection_records();
        if (g_world_menu_selection_results[0] == 0) {
            if (g_world_shop_fur_has_poached_stock != 0) {
                g_world_shop_item_category = world_menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
                g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_LIST;
                g_world_shop_fur_opened = 0;
            } else {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82E, 0);
                g_world_shop_fur_waiting_message = 1;
            }
        } else if (g_world_menu_selection_results[0] == 1) {
            if (world_menu_has_items(0) != 0) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_SELL_LIST;
                g_world_shop_fur_opened = 0;
            } else {
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF845, 0);
                g_world_shop_fur_waiting_message = 1;
            }
        } else {
            g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT_FADE;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF831, 0);
            g_world_shop_fur_opened = 0;
        }
    }
    world_shop_run_obtain_gil_menu();
}

/* Query the selected poached-stock entry with a 0 quantity adjustment.
 *
 * An empty menu returns 0. */
static s32 world_shop_get_selected_poached_item_quantity(s32 unused_row) {
    if (g_menu_list_entry_count != 0)
        return world_shop_adjust_poached_item_quantity(g_item_list_entries[g_menu_list_selected_index], 0);
    return 0;
}

/* Fur shop: the list of the party's own inventory to sell; sibling of
 * world_shop_run_fur_buy_list_step.
 *
 * The callback block is written in ascending address order (794 -> 7ac), as
 * the sibling writes it. That makes gcc materialise the two twice-used
 * addresses (count_2 in a0, count in v1) up front at 0xb0-0xbc and defer
 * their stores; the target's emission order then follows. First-use order,
 * not store order, decides this.
 */
void world_shop_run_fur_sell_list_step(void) {
    s32 message;

    if (g_world_shop_fur_sell_list_initialized == 0) {
        message = world_menu_has_items(0) != 0 ? 0xf840 : 0xf845;

        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, message, 0);
        g_world_shop_item_category = world_menu_init_cursor_position(WORLD_MENU_CURSOR_SHOP_ITEM_TAB, 0);
        world_item_build_category_list(-1, g_world_shop_item_category + 1,
            g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_init_scrollable_list_core_variant(
            g_item_list_entries, 0, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
        g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
        g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4].value = world_item_get_selected_half_price;
        g_world_menu_script_callbacks[5].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[6].value = menu_get_entry_item_count;
        g_world_shop_fur_sell_list_initialized = 1;
    }

    menu_set_vsync_mode_and_event_speed(2);
    if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) != 0) {
        g_world_shop_fur_sell_list_initialized = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf830, 0);
        g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_MENU;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
    } else if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) != 0) {
        if (g_menu_list_entry_count == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_SELL_CONFIRM;
            g_world_shop_quantity_menu_data[1].selected_index = 0;
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
    }
    world_menu_switch_item_category(g_menu_input_newly_pressed);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_sale_confirm_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    world_shop_run_obtain_gil_menu();
}

/* Fur shop: confirm a sale. The party receives half price, and the sold copies join the
 * poached stock. */
void world_shop_run_fur_sell_confirm_step(void) {
    s32 result = 1;

    if (g_world_shop_fur_sale_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_fur_sale_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF841, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_SELL_LIST;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_fur_sell_quantity_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(world_item_get_half_price(g_item_list_entries[g_menu_list_selected_index])
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_shop_adjust_poached_item_quantity(g_item_list_entries[g_menu_list_selected_index],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF837, 0);
            item_rebuild_category_list(g_world_shop_item_category);
            world_item_build_category_list(0, g_world_shop_item_category + 1,
                g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            world_menu_init_and_load_scrollable_list(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_world_shop_sale_confirm_menu_script);
            menu_set_window_scale_step(10);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_fur_sale_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_sale_confirm_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    world_shop_run_obtain_gil_menu();
}

/* Fur shop: the quantity prompt of a sale, capped by the inventory count. */
static s32 world_shop_run_fur_sell_quantity_menu(void) {
    s32 count;

    if (g_world_shop_fur_quantity_menu_running == 0) {
        g_world_shop_sell_total_shown = 0;
        g_world_shop_sell_quantity_item = g_item_list_entries[g_menu_list_selected_index];
        g_world_menu_prompt_active = 1;
        count = item_adjust_inventory_count(g_world_shop_sell_quantity_item, 0);
        if (count == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF842, 0);
            return -1;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF843, 0);
        g_world_shop_quantity_limit = count;
        g_world_shop_sell_total_shown = 0;
        g_sound_suppressed = 1;
    }
    g_world_shop_fur_quantity_menu_running
        = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_quantity_menu_data);
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_world_shop_sell_total_shown == 0) {
        g_text_substitution_values[0] = world_item_get_half_price(g_world_shop_sell_quantity_item)
            * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY];
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF844, 0);
        g_world_shop_sell_total_shown = 1;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_world_shop_sell_total_shown != 0) {
        g_world_shop_sell_total_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF843, 0);
    }
    if (g_world_shop_fur_quantity_menu_running == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_world_shop_fur_quantity_menu_running;
}

static s32 world_menu_get_item_half_price_display_value(s32 entry_index) {
    s32 display_value = world_item_get_price(g_item_list_entries[entry_index]) >> 1;

    if ((g_item_list_entries[entry_index] & WORLD_ITEM_LIST_ENTRY_UNEQUIPPABLE) != 0)
        display_value |= MENU_NUMBER_VALUE_DISABLED;
    return display_value;
}

/* Fur shop: the list of the shop's poached stock, bought at half price.
 *
 * Mirrors world_shop_handle_item_purchase_menu with the half-price value and
 * the poached-stock quantity as callbacks. */
void world_shop_run_fur_buy_list_step(void) {
    s16 x;
    s16 y;
    s32 input;
    s32 disabled;
    s8 result;

    if (g_world_shop_fur_purchase_list_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF832, 0);
        g_world_shop_fur_purchase_list_initialized = 1;
        world_shop_build_item_list(-1, g_world_menu_context_id,
            g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_get_selection_record_variant(g_world_shop_item_category, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
        g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
        g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4].value = world_menu_get_item_half_price_display_value;
        g_world_menu_script_callbacks[5].value = world_item_count_selected_equipped_by_party;
        g_world_menu_script_callbacks[6].value = world_item_count_selected_owned_and_equipped;
        g_world_menu_script_callbacks[8].value = world_shop_get_selected_poached_item_quantity;
        g_world_shop_fur_equip_check_pending = 0;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (g_world_shop_fur_equip_check_pending != 0) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        result = thread_is_running(THREAD_SCREEN_SLOT_MESSAGE);
        g_world_shop_fur_equip_check_pending = result;
        if (result == 0) {
            menu_set_selection_record(
                g_world_shop_item_category, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_EQUIP;
            g_world_shop_fur_purchase_list_initialized = 0;
        }
        menu_input_clear_state();
    } else {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CROSS) {
            g_world_shop_fur_purchase_list_initialized = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_MENU;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF830, 0);
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        } else if ((input & PSX_PAD_TRIANGLE) && g_menu_list_entry_count != 0 && g_world_shop_item_category != 4) {
            g_world_shop_fur_equip_check_pending = 1;
        } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0) {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_CONFIRM;
                g_world_shop_quantity_menu_data[1].selected_index = 0;
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            }
        }
    }
    disabled = 0;
    world_menu_switch_item_category(g_menu_input_secondary_repeat);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_fur_purchase_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    if (world_map_is_busy() != 0 || g_menu_list_entry_count == 0 || g_world_shop_item_category == 4) {
        disabled = 1;
    }
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
}

/* Fur shop sibling of world_menu_confirm_item_purchase: the party pays half
 * price and the poached stock is reduced. */
void world_shop_run_fur_buy_confirm_step(void) {
    s32 result = 1;

    if (g_world_shop_fur_purchase_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_world_shop_fur_purchase_message_wait = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF83B, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_LIST;
            g_world_menu_prompt_active = 0;
        }
    } else {
        result = world_shop_run_fur_buy_quantity_menu();
    }
    if (result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            world_shop_adjust_gil(-(world_item_get_price(g_item_list_entries[g_menu_list_selected_index]) >> 1)
                * g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index],
                g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            world_shop_adjust_poached_item_quantity(g_item_list_entries[g_menu_list_selected_index],
                -g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY]);
            item_rebuild_category_list(g_world_shop_item_category);
            world_shop_build_item_list(-1, g_world_menu_context_id,
                g_world_shop_item_category != 4 ? g_world_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            world_menu_init_and_load_scrollable_list(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_world_shop_fur_purchase_menu_script);
            menu_set_window_scale_step(10);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF837, 0);
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF838, 0);
        }
    }
    if (result <= 0) {
        g_world_shop_fur_purchase_message_wait = 1;
    }
    world_menu_switch_item_category(0);
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_fur_purchase_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C, 0, 1, MAIN_SFX_CONFIRM);
    world_shop_run_obtain_gil_menu();
}

/* Fur shop twin of world_shop_run_item_purchase_quantity_menu: the quantity
 * prompt is capped by the 99-item inventory limit, the gil on hand at half
 * price and the poached stock. */
static s32 world_shop_run_fur_buy_quantity_menu(void) {
    s32 owned;
    s32 count;
    s32 price;
    s32 room;
    s32 stock;

    if (g_world_shop_fur_purchase_result == 0) {
        g_world_shop_fur_purchase_confirm_shown = 0;
        g_world_shop_fur_purchase_item = g_item_list_entries[g_menu_list_selected_index];
        g_world_menu_prompt_active = 1;
        owned = item_get_total_count_variant(g_world_shop_fur_purchase_item);
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
        = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, g_world_shop_quantity_menu_data);
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_world_shop_fur_purchase_confirm_shown == 0) {
        g_text_substitution_values[0]
            = g_world_menu_selection_results[WORLD_MENU_SELECTION_SHOP_QUANTITY] * g_world_shop_fur_purchase_price;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF836, 0);
        g_world_shop_fur_purchase_confirm_shown = 1;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
    }
    if (thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_world_shop_fur_purchase_confirm_shown != 0) {
        g_world_shop_fur_purchase_confirm_shown = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF835, 0);
    }
    if (g_world_shop_fur_purchase_result == 0) {
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 0) {
            g_sound_queued_effect_id = MAIN_SFX_SHOP_TRANSACTION;
        }
        if (g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT] == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        g_sound_suppressed = 0;
    }
    return g_world_shop_fur_purchase_result;
}

void world_shop_run_fur_equip_purchase_step(void) {
    world_shop_run_equip_candidate_step();
    if (g_world_shop_menu_step == WORLD_SHOP_STEP_BUY_LIST)
        g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_BUY_LIST;
}

/* Soldier Office hire step: runs the hire menu and charges for the new
 * unit's starting equipment. */
void world_shop_run_soldier_office_hire_menu_step(void) {
    unit_stats_t unit;
    s32 i;
    s32 result;
    s32 slot;

    if (g_world_shop_hire_menu_initialized == 0) {
        g_world_shop_hire_confirm_menu.selected_index = 0;
        g_world_shop_hire_fee_confirm_menu.selected_index = 0;
        g_world_shop_hire_result_shown = 0;
        g_world_shop_hire_menu_initialized = 1;
        g_menu_status_display_thread_params.y = 0;
        D_801CD0EC = 0;
        D_801CD0F4 = 0;
        g_world_formation_unit_banner_enabled = 0;
        g_world_shop_party_unit_count = 0;
        for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
            result = main_unit_init_job_data(&unit, i, 1);
            if (result == 0 && unit.formation_index < PARTY_GUEST_SLOT_FIRST) {
                g_world_shop_party_unit_count++;
            }
        }
    }
    if (g_world_shop_hire_result_shown != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        }
    } else {
        i = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_shop_hire_menu_thread_data);
        if (i == 0) {
            if (g_world_menu_selection_results[0] == 3 || g_world_menu_selection_results[0] == -1) {
                g_world_shop_hire_menu_initialized = 0;
                g_world_shop_menu_step = WORLD_SHOP_STEP_EXIT_FADE;
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF825, 0);
                world_item_cash_out_excess_inventory();
            } else if (g_world_menu_selection_results[0] == 2) {
                g_world_formation_unit_count
                    = world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_MONSTERS);
                if (g_world_formation_unit_count != 0) {
                    g_world_shop_menu_step = WORLD_SHOP_STEP_RENAME_SELECT_UNIT;
                    i = 0xF847;
                } else {
                    i = 0xF846;
                    g_world_shop_hire_result_shown = 1;
                }
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, i, 0);
            } else {
                slot = world_formation_generate_unit(g_world_menu_selection_results[0]);
                if (slot >= 0) {
                    g_world_shop_service_fee = 1000;
                    for (; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                        g_world_shop_service_fee += world_item_get_price(g_world_unit_pointers[slot]->equipment[i]);
                    }
                    if (world_shop_adjust_gil(0) < g_world_shop_service_fee) {
                        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF823, 0);
                        main_party_remove_unit(g_world_unit_pointers[slot]->identity.roster_slot);
                        g_world_shop_hire_result_shown = 1;
                    } else {
                        g_unit_selected_index = slot;
                        world_formation_stage_selected_unit();
                        g_world_formation_unit_banner_at_bottom = 0;
                        g_world_formation_unit_banner_at_bottom_latch = 0;
                        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF824, 0);
                        g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_PREVIEW;
                    }
                } else {
                    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF822, 0);
                    g_world_shop_hire_result_shown = 1;
                }
            }
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}

/*
 * Soldier office step that shows the generated recruit.
 *
 * Any face button dismisses the message box; once it has closed, unit
 * browsing and the status banner are turned on for the preview. A further
 * face button closes the panels and moves on to the hire confirmation
 * (message 0xF826).
 */
void world_shop_run_recruit_unit_preview_step(void) {
    if (g_world_shop_recruit_preview_initialized == 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_world_formation_unit_browse_enabled = 1;
            g_world_formation_unit_banner_enabled = 1;
            g_world_formation_unit_banner_at_bottom = 0;
            g_menu_status_display_thread_params.y = 0;
            g_menu_help_message_text_id = MENU_GRID_HELP_RECRUIT_PREVIEW;
            g_world_shop_recruit_preview_initialized = 1;
        }
        world_shop_run_obtain_gil_menu();
        world_shop_run_soldier_office_fee_menu_script();
        world_menu_run_script_with_callback_suppressed();
    } else if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
        g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_CONFIRM;
        g_world_formation_unit_browse_enabled = 0;
        g_world_formation_unit_banner_enabled = 0;
        menu_toggle_numeric_editor_thread(0);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF826, 0);
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        g_world_shop_recruit_preview_initialized = 0;
    }
}

/*
 * Soldier office: confirm hiring the previewed recruit.
 *
 * Selection 1 or -1 declines and removes the recruit from the roster;
 * selection 2 returns to the preview. Any other result shows a message and
 * waits for its thread before going on to the name entry.
 */
void world_shop_run_hire_confirm_step(void) {
    s16 selection;
    if (g_world_shop_hire_confirm_message_wait != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_world_shop_hire_confirm_message_wait = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_NAME_ENTRY;
        }
    } else if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_shop_hire_confirm_menu) == 0) {
        selection = g_world_menu_selection_results[2];
        if (selection == 1 || selection == -1) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF827, 0);
            g_world_shop_hire_menu_initialized = 0;
            main_party_remove_unit(g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot);
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
        } else if (selection == 2) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_PREVIEW;
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF829, 0);
            g_world_shop_hire_confirm_message_wait = 1;
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}

/* Soldier office: the name entry keyboard for the recruit, then the fee prompt. */
void world_shop_run_hire_name_entry_step(void) {
    if (g_world_shop_hire_name_entry_initialized == 0) {
        g_world_formation_unit_banner_enabled = 0;
        menu_toggle_unit_status_thread(0);
        g_world_shop_hire_name_entry_initialized = 1;
    }
    g_world_shop_entered_unit_name
        = world_name_run_entry_keyboard_frame(g_world_unit_pointers[g_unit_selected_index]->name, 0);
    if (g_world_shop_entered_unit_name != 0) {
        g_world_shop_hire_name_entry_initialized = 0;
        g_text_substitution_values[0] = g_world_shop_service_fee;
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82A, 0);
        g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_FEE_CONFIRM;
    }
}

/*
 * Soldier office: confirm the hiring fee.
 *
 * Choice 0 pays the fee and stores the recruit's name; 1 or -1 removes the
 * recruit from the roster. Other choices return to the name entry.
 */
void world_shop_run_hire_fee_confirm_step(void) {
    s16 choice;
    g_menu_help_message_text_id = 0;
    if (g_world_shop_hire_fee_message_wait != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
            g_world_shop_hire_fee_message_wait = 0;
        }
    } else if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_shop_hire_fee_confirm_menu) == 0) {
        choice = g_world_menu_selection_results[WORLD_MENU_SELECTION_PROMPT_RESULT];
        if (choice == 0) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf82b, 0);
            world_shop_adjust_gil(-g_world_shop_service_fee);
            world_formation_set_unit_name(g_unit_selected_index, g_world_shop_entered_unit_name);
            g_world_shop_hire_fee_message_wait = 1;
        } else if (choice == 1 || choice == -1) {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf827, 0);
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
            main_party_remove_unit(g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot);
            g_world_shop_hire_menu_initialized = 0;
        } else {
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_world_shop_hire_fee_confirm_menu.selected_index = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_NAME_ENTRY;
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}

/* Return to the soldier office menu with message 0xf821, redrawing the gil,
 * fee and roster-count displays for this frame. */
void world_shop_reopen_soldier_office_menu_step(void) {
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf821, 0);
    g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
    g_world_shop_hire_menu_initialized = 0;
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}

/* Menu callback: no marker (-1) for any unit. */
static s32 world_shop_rename_unit_get_unit_marker(s32 unused_unit_index) {
    return -1;
}

/*
 * Menu step for the rename-unit prompt: on the first frame either leaves
 * the screen when no face button is held, or arms the prompt. Confirm
 * opens the name entry (mode 1 when a unit is already renamed) unless the
 * unit has Reis's dragon identity, which refuses with message 0x1B; cancel backs
 * out with message 0x19.
 */
void world_shop_run_rename_unit_prompt_step(void) {
    s32 mode;
    s32 input;
    s32 text_id;

    if (g_world_shop_rename_prompt_initialized == 0) {
        if (!(g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE))) {
            world_shop_run_obtain_gil_menu();
            world_shop_run_soldier_office_fee_menu_script();
            world_menu_run_script_with_callback_suppressed();
            return;
        }
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_world_shop_rename_error_shown = 0;
        g_world_shop_rename_prompt_initialized = 1;
        world_formation_init_cursor_and_scroll_state(0);
        g_world_shop_background_visible = 0;
    }
    if (g_world_formation_unit_banner_enabled != 0) {
        text_id = MENU_GRID_HELP_BANNER_TOP;
        if (g_world_formation_unit_banner_at_bottom != 0) {
            text_id = MENU_GRID_HELP_BANNER_BOTTOM;
        }
        g_menu_help_message_text_id = text_id;
    } else {
        g_menu_help_message_text_id = 0;
    }
    world_formation_update_and_draw_unit_grid(
        1, 1, 0, g_menu_input_secondary_repeat, g_menu_input_lock_mode, world_shop_rename_unit_get_unit_marker, 0);
    if (g_world_shop_rename_error_shown != 0) {
        g_menu_help_message_text_id = -1;
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_rename_error_shown = 0;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0 && world_formation_is_unit_list_scroll_needed() == 0) {
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_CIRCLE) {
            if (g_world_unit_pointers[g_unit_selected_index]->sprite_set == CHARACTER_IDENTITY_REIS_DRAGON) {
                if (g_world_formation_unit_banner_at_bottom != 0) {
                    mode = 1;
                } else {
                    mode = 2;
                }
                thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
                thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, mode | 0x18, 0xF848, 0);
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
                g_world_shop_rename_error_shown = 1;
            } else {
                g_world_shop_menu_step = WORLD_SHOP_STEP_RENAME_NAME_ENTRY;
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                g_world_formation_unit_banner_enabled = 0;
                g_world_shop_rename_prompt_initialized = 0;
            }
        } else if (input & PSX_PAD_CROSS) {
            g_world_shop_background_visible = 1;
            g_world_shop_rename_prompt_initialized = 0;
            g_world_formation_unit_banner_enabled = 0;
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF821, 0);
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        }
    }
}

/* Soldier office: the name entry keyboard of the Rename service; sibling of
 * world_shop_run_hire_name_entry_step, which goes on to the fee prompt (0xF82A). */
void world_shop_run_rename_name_entry_step(void) {
    if (g_world_shop_rename_name_entry_initialized == 0) {
        g_world_formation_unit_banner_enabled = 0;
        menu_toggle_unit_status_thread(0);
        g_world_shop_rename_name_entry_initialized = 1;
    }
    world_formation_update_and_draw_unit_grid(
        1, 0, 0, 0, g_menu_input_lock_mode, world_shop_rename_unit_get_unit_marker, 0);
    g_world_shop_entered_unit_name
        = world_name_run_entry_keyboard_frame(g_world_unit_pointers[g_unit_selected_index]->name, 0);
    if (g_world_shop_entered_unit_name != 0) {
        g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        world_formation_set_unit_name(g_unit_selected_index, g_world_shop_entered_unit_name);
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF821, 0);
        g_world_shop_background_visible = 1;
        g_world_shop_rename_name_entry_initialized = 0;
    }
}

/* Menu callback: the hiring fee for the highlighted row of the hire menu,
 * 1500 for row 0, 1400 for row 1 and 0 otherwise. */
static s32 world_shop_get_soldier_office_entry_fee(s32 unused_row) {
    s16 mode = g_world_shop_hire_menu_thread_data.selected_index;

    if (mode == 0) {
        return 0x5DC;
    }
    return (mode == 1) ? 0x578 : 0;
}

/* Run the soldier office's fee display script with
 * world_shop_get_soldier_office_entry_fee installed as menu value callback 0,
 * then restore the previous callback. */
static void world_shop_run_soldier_office_fee_menu_script(void) {
    menu_callback_t* callback_slot;
    s32 (*previous_callback)(s32);

    callback_slot = g_world_menu_script_callbacks;
    previous_callback = callback_slot->value;
    callback_slot->value = world_shop_get_soldier_office_entry_fee;
    menu_run_script_with_palette_mode_variant(&g_world_shop_soldier_office_fee_menu_script, 0, 0);
    callback_slot->value = previous_callback;
}

/* Display the party unit count directly while value callback 0 is disabled. */
static void world_menu_run_script_with_callback_suppressed(void) {
    s32 (*previous_callback)(s32);

    previous_callback = g_world_menu_script_callbacks[0].value;
    g_world_menu_script_callbacks[0].value = 0;
    g_world_shop_roster_count_menu_script.unit_count.value = g_world_shop_party_unit_count;
    /* The serialized script prefix shares the bytecode interpreter's instruction view. */
    menu_run_script_with_palette_mode_variant((menu_instruction_t*)&g_world_shop_roster_count_menu_script, 0, 0);
    g_world_menu_script_callbacks[0].value = previous_callback;
}
