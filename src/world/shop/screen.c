/* The item shop runs the shared shop screen and its buying and selling menus. */

#include "fft/screen.h"
#include "fft/equipment.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/formation_item.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/item_menu.h"
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
#include "fft/script_variable.h"
#include "fft/shop.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_screen_text_shop_menu_labels_data[];
extern menu_entry_t g_shop_main_menu_thread_data;
extern u8 g_shop_background_visible;

/* Refusals of buy_quantity_update and
 * purchase_quantity_update; otherwise they return whether the
 * quantity prompt is still open. */
enum {
    SHOP_QUANTITY_HOLDING_MAX = -2, /* the party already holds 99 */
    SHOP_QUANTITY_CANNOT_AFFORD = -1
};
extern u8 g_shop_item_category;
extern menu_entry_t g_shop_quantity_menu_data[2];
extern u8 g_shop_quantity_menu_running;
extern s8 g_shop_buy_list_initialized;
static s8 s_shop_buy_equip_check_pending;
extern s16 g_shop_buy_message_wait;
extern s16 g_shop_buy_item;
extern s32 g_shop_buy_unit_price;
extern u8 g_shop_buy_total_shown;
extern u8 g_shop_buy_equip_initialized;
extern u8 g_shop_buy_equip_preview_visible;
extern s16 g_shop_buy_equip_item;
extern s16 g_shop_buy_equip_last_unit;
extern u8 g_shop_buy_equip_saved_browse;
extern s8 g_shop_sell_list_initialized;
extern s16 g_shop_sell_message_wait;
extern u8 g_shop_sell_result; /* sell-menu thread result */

extern u16 g_shop_sell_item; /* item being sold */

extern u8 g_shop_sell_confirm_shown; /* confirmation prompt shown */

extern u8 g_shop_fitting_cost_visible;
extern menu_entry_t g_fur_shop_menu_thread_data;

/* The cursor doubles as the soldier-office mode (hire_fee_get). */
extern menu_entry_t g_soldier_office_hire_menu_thread_data;

/* Byte-aligned display streams; each extent includes the terminating opcode. */
extern u8 g_shop_war_funds_script[34];
extern u8 g_shop_buy_menu_script[227];
extern u8 g_shop_sell_list_menu_script[235];

/* The quantity menu passes this range to screen_labeled_number_entry_thread. */
extern screen_number_range_t g_shop_quantity_range;

static void shop_menu_update(void);
static void shop_exit_update(void);
static void buy_list_update(void);
static void buy_confirm_update(void);
static s32 buy_quantity_update(void);

static s32 sell_quantity_update(void);

/* shop_step_e indexes the original 28-entry screen dispatcher. */
static void (*s_shop_step_handlers[])(void) = {
    shop_menu_update,
    buy_list_update,
    buy_confirm_update,
    shop_buy_unit_preview_update,
    shop_sell_list_update,
    shop_sell_confirm_update,
    shop_fitting_menu_update,
    shop_fitting_equipment_update,
    shop_fitting_best_fit_update,
    shop_fitting_remove_update,
    shop_fitting_equipped_sale_update,
    shop_fitting_inventory_sale_update,
    shop_fitting_checkout_update,
    shop_exit_update,
    fur_shop_menu_update,
    fur_shop_buy_list_update,
    fur_shop_buy_confirm_update,
    fur_shop_buy_unit_preview_update,
    fur_shop_sell_list_update,
    fur_shop_sell_confirm_update,
    soldier_office_menu_update,
    soldier_office_hire_preview_update,
    soldier_office_hire_confirm_update,
    soldier_office_hire_name_update,
    soldier_office_hire_fee_update,
    soldier_office_menu_reopen,
    soldier_office_rename_unit_update,
    soldier_office_rename_name_update,
};

/* Backdrop and VRAM rectangles, followed by the main menu and exit gates. */
static RECT s_shop_background_rect = { 0, 64, 255, 128 };

static RECT s_shop_backdrop_tile_rect = { 0, 0, 256, 240 };

static RECT s_shop_edge_tile_rect = { 255, 0, 1, 240 };

static RECT s_shop_background_vram_rect = { 768, 0, 256, 128 };

static RECT s_shop_vram_backup_rect = { 0, 0, 256, 128 };

static u8 s_shop_main_menu_running = 0;

static u8 s_shop_exit_fade_started = 0;

static u8 s_shop_exit_step_initialized = 0;

/* Run a shop screen (item shop, Fur Shop or Soldier Office): set the primitive budgets,
 * load the town's backdrop and the shop music, then run frames through the step handlers
 * until the exit step, saving the party records and restoring VRAM on the way out. */
void shop_screen_run(s32 shop_type) {
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
    RECT* tile_box = &s_shop_edge_tile_rect;
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
    buttons = script_variable_get(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
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
        g_screen_context_id = script_variable_get(EVENT_SCRIPT_VAR_LOCATION);
        ot_index = MUSIC_SHOP;
        disabled = 0xF801;
        tim_index = 0;
        g_shop_menu_step = SHOP_STEP_MAIN_MENU;
    }

    /* After the backdrop is in VRAM, the TIM buffer is reused for a VRAM backup that the exit loads back. */
    SetDispMask(0);
    while (DrawSync(1) != 0) { }
    gfx_move_image_and_wait(&s_shop_background_vram_rect, 0, 0);
    tim = shop_background_load(tim_index + buttons);
    gfx_load_image_and_wait(&s_shop_background_vram_rect, tim + 5);
    gfx_store_image_and_wait(&s_shop_vram_backup_rect, tim);

    screen_packet_buffers_init(buffers, otags, 0, 0, 0, textured_quads, 0, 0, gradient_quads, textured_gradient_quads,
        flat_lines, 0, 0, 0, 0, 0, tiles, 0, 0, 0, 0, 0, 0, draw_moves, draw_areas, draw_modes);
    last_step = -1;
    formation_menu_state_init();
    g_shop_main_menu_thread_data.selected_index = 0;
    g_fur_shop_menu_thread_data.selected_index = 0;
    g_soldier_office_hire_menu_thread_data.selected_index = 0;

    screen_text_section_bind(SCREEN_TEXT_DATA_SHOP_DIALOGUE);
    g_text_section_pointers[SCREEN_TEXT_SECTION_SHOP_MENU_LABELS] = g_screen_text_shop_menu_labels_data;
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
    thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, disabled, 0);
    g_screen_music_slot = music_free_slot_load(ot_index);
    music_track_switch(g_screen_music_slot, 0x7F, 0);
    g_shop_background_visible = 1;
    g_formation_unit_banner_enabled = 0;

    while (g_shop_menu_step != SHOP_STEP_EXIT) {
        screen_frame_present(0, 0);
        menu_input_message_state_update();
        screen_input_lockout_update();
        screen_fade_in_update();
        if (g_menu_input_primary_repeat & PSX_PAD_SELECT) {
            screen_help_text_start(DIALOG_POSITION_CENTER);
        }
        if (g_screen_grid_id != MENU_GRID_HELP_NONE) {
            screen_grid_cursor_update(g_formation_unit_pointers[g_unit_selected_index]->detail.uses_monster_skillset);
        }
        screen_palette_mode_set(g_menu_input_lock_mode);

        if (g_shop_background_visible != 0) {
            menu_gfx_enqueue_textured_quad(&s_shop_background_rect, 0, 0, 0, 0, 0x14C, 0, 1);
            gfx_enqueue_tile(&s_shop_backdrop_tile_rect, &g_screen_black_tile_color, 0, 0);
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
            menu_pressable_button_draw(
                MENU_PRESSABLE_BUTTON_L1, 0xC, 0xC, buttons & PSX_PAD_L1, disabled, SOUND_SFX_PAGE_SWITCH);
            menu_pressable_button_draw(
                MENU_PRESSABLE_BUTTON_R1, 0xE0, 0xC, buttons & PSX_PAD_R1, disabled, SOUND_SFX_PAGE_SWITCH);
        }

        s_shop_step_handlers[g_shop_menu_step]();
        if (g_shop_menu_step != SHOP_STEP_RENAME_NAME_ENTRY && g_shop_menu_step != SHOP_STEP_HIRE_NAME_ENTRY
            && g_shop_menu_step != SHOP_STEP_FITTING_ROOM_REMOVE) {
            formation_selection_stage_update();
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
        menu_input_event_state_update(
            g_menu_gfx_context->otag + ot_index, screen_buttons_filter(), g_screen_draw_buffer_clip_y == 0 ? 0xF0 : 0);

        /* Entering a new step resets the per-step menu state. */
        if (g_shop_menu_step != last_step) {
            g_formation_unit_cycle_mode = 0;
            menu_set_vsync_mode_and_event_speed(0);
            g_help_message_text_id = 0;
            screen_selection_results_reset();
            last_step = g_shop_menu_step;
        }
        screen_sound_dispatch_effect();
    }

    music_scenario_slot_unload(g_screen_music_slot);
    formation_save_records();
    menu_set_vsync_mode_and_event_speed(0);
    screen_context_reset();
    gfx_load_image_and_wait(&s_shop_background_vram_rect, tim);
    heap_free(tim);
}

/* The on_confirm callback of the shop's quantity range: nothing to do. */
void shop_noop_callback(void) {
}

/* Shop main menu step: runs the Buy / Sell / Fitting Room menu and, when it
 * closes, moves to the chosen step (rows 0..2; the fitting room also sets its
 * message), or to the exit fade with its message for anything else. */
static void shop_menu_update(void) {
    u8 running;

    if (s_shop_main_menu_running == 0) {
        g_formation_unit_cycle_mode = 0;
        g_menu_list_selected_index = 0;
    }
    running = menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, &g_shop_main_menu_thread_data);
    s_shop_main_menu_running = running;
    if (running == 0) {
        menu_selection_records_clear();
        if (g_screen_selection_results[0] == 0) {
            g_shop_menu_step = SHOP_STEP_BUY_LIST;
            g_shop_item_category = screen_cursor_position_init(SCREEN_CURSOR_SHOP_ITEM_TAB, 0);
        } else if (g_screen_selection_results[0] == 1) {
            g_shop_menu_step = SHOP_STEP_SELL_LIST;
        } else if (g_screen_selection_results[0] == 2) {
            g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
            g_unit_selected_index = 0;
            formation_stage_selected_unit();
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF818, 0);
        } else {
            g_shop_menu_step = SHOP_STEP_EXIT_FADE;
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF803, 0);
        }
    }
    shop_gil_draw();
}

/* Menu screen step: on the first frame reset the fade flag; once any of the
 * face buttons (0xf0) is pressed start the fade-out with a music fade, then
 * wait for the fade to complete before releasing the screen (g_shop_menu_step = -1). */
static void shop_exit_update(void) {
    if (s_shop_exit_step_initialized == 0) {
        s_shop_exit_fade_started = 0;
        s_shop_exit_step_initialized = 1;
    }
    if (s_shop_exit_fade_started != 0) {
        if (screen_fade_out_update() == 0) {
            g_shop_menu_step = SHOP_STEP_EXIT;
            s_shop_exit_step_initialized = 0;
        }
    } else if (g_menu_input_newly_pressed & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
        screen_fade_out_start();
        music_volume_target_set(0, 0xF0);
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        s_shop_exit_fade_started = 1;
    }
    shop_gil_draw();
    if (g_screen_context_id == SCREEN_CONTEXT_SOLDIER_OFFICE) {
        soldier_office_fee_draw();
        soldier_office_unit_count_draw();
    }
}

/* Run the shop's war-funds menu script with shop_gil_adjust installed as the
 * script callback, then put the previous callback back. The step handlers call it. */
void shop_gil_draw(void) {
    menu_callback_t* callback_slot;
    s32 (*previous_callback)(s32);

    callback_slot = g_menu_row_callbacks;
    previous_callback = callback_slot->value;
    callback_slot->value = shop_gil_adjust;
    menu_run_script_with_palette_mode(g_shop_war_funds_script, 0, 0);
    callback_slot->value = previous_callback;
}

/* Returns the owned-and-equipped count for the selected item. */
s32 shop_buy_owned_count_get(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count != 0) {
        result = item_total_count_get(g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}

/* Returns the party-equipped count for the selected item. */
s32 shop_buy_equipped_count_get(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count != 0) {
        result = item_equipped_count_get(g_item_list_entries[g_menu_list_selected_index] & ITEM_ENTRY_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}

/* Full-price twin of poached_price_get: the menu
 * item's price with bit 30 copied from its 0x4000 entry flag. */
s32 shop_buy_price_get(s32 entry_index) {
    s32 display_value = shop_item_price_get(g_item_list_entries[entry_index]);

    if ((g_item_list_entries[entry_index] & ITEM_ENTRY_UNEQUIPPABLE) != 0)
        display_value |= MENU_NUMBER_VALUE_DISABLED;
    return display_value;
}

/* Switch the active item category and restore its saved list cursor. */
void shop_switch_item_tab(s32 input) {
    s16 selected_index;
    s16 scroll_base_index;
    s32 category;
    s32 cursor_slot;
    s16 item;
    s32 description;

    category = menu_selection_horizontal_sound_step(5, SCREEN_CURSOR_SHOP_ITEM_TAB, input, SOUND_SFX_PAGE_SWITCH);
    cursor_slot = g_shop_item_category;
    if (cursor_slot != category) {
        if (g_shop_menu_step != SHOP_STEP_BUY_LIST && g_shop_menu_step != SHOP_STEP_FUR_BUY_LIST) {
            cursor_slot += 5;
        }
        menu_selection_record_set(
            cursor_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_shop_item_category = category;
        if (g_shop_menu_step == SHOP_STEP_BUY_LIST || g_shop_menu_step == SHOP_STEP_FUR_BUY_LIST) {
            shop_stock_list_build(-1, g_screen_context_id,
                category != ITEM_MENU_TAB_OTHER ? (category + 1) : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
                ITEM_LIST_MODE_ALL);
            cursor_slot = g_shop_item_category;
        } else {
            formation_item_list_build(-1, g_shop_item_category + 1,
                category != ITEM_MENU_TAB_OTHER ? category + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
                ITEM_LIST_MODE_ALL);
            cursor_slot = g_shop_item_category + 5;
        }
        menu_selection_record_restore(cursor_slot, &selected_index, &scroll_base_index, g_item_list_entries);
        menu_init_scrollable_list(
            g_item_list_entries, selected_index, scroll_base_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
    }
    screen_item_tabs_draw(1, g_shop_item_category, g_menu_input_lock_mode);
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
static void buy_list_update(void) {
    s16 selected_index;
    s16 scroll_base_index;
    s32 input;
    s32 disabled;
    s8 result;

    if (g_shop_buy_list_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        }
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF804, 0);
        g_shop_buy_list_initialized = 1;
        shop_stock_list_build(-1, g_screen_context_id,
            g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_selection_record_restore(g_shop_item_category, &selected_index, &scroll_base_index, g_item_list_entries);
        menu_init_scrollable_list(
            g_item_list_entries, selected_index, scroll_base_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[1].value = equipment_get_row_item_count;
        g_menu_row_callbacks[2].icon = item_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
        g_menu_row_callbacks[4].value = shop_buy_price_get;
        g_menu_row_callbacks[5].value = shop_buy_equipped_count_get;
        g_menu_row_callbacks[6].value = shop_buy_owned_count_get;
        s_shop_buy_equip_check_pending = 0;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (s_shop_buy_equip_check_pending != 0) {
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        result = thread_is_running(THREAD_SCREEN_SLOT_MESSAGE);
        s_shop_buy_equip_check_pending = result;
        if (result == 0) {
            g_shop_buy_list_initialized = 0;
            menu_selection_record_set(
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
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF802, 0);
        } else if ((input & PSX_PAD_TRIANGLE) && g_menu_list_entry_count != 0
            && g_shop_item_category != ITEM_MENU_TAB_OTHER) {
            s_shop_buy_equip_check_pending = 1;
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
    menu_run_script_with_palette_mode(g_shop_buy_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    if (screen_busy_check() != 0 || g_menu_list_entry_count == 0 || g_shop_item_category == ITEM_MENU_TAB_OTHER) {
        disabled = 1;
    }
    menu_pressable_button_draw(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, SOUND_SFX_CONFIRM);
    shop_gil_draw();
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
static void buy_confirm_update(void) {
    s32 result = 1;

    if (g_shop_buy_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_shop_buy_message_wait = 0;
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF805, 0);
            g_screen_prompt_active = 0;
            g_shop_menu_step = SHOP_STEP_BUY_LIST;
        }
    } else {
        result = buy_quantity_update();
    }
    if (result == 0) {
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            shop_gil_adjust(-shop_item_price_get(g_item_list_entries[g_menu_list_selected_index])
                * g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            item_inventory_count_adjust(g_item_list_entries[g_menu_list_selected_index],
                g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF811, 0);
            item_rebuild_category_list(g_shop_item_category);
        } else {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_shop_buy_message_wait = 1;
    }
    shop_switch_item_tab(0);
    menu_run_script_with_palette_mode(g_shop_buy_menu_script, 0, g_menu_input_lock_mode);
    menu_pressable_button_draw(MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE, 0xBA, 0x4C, 0, 1, SOUND_SFX_CONFIRM);
    shop_gil_draw();
}

/* Runs the shop purchase-quantity menu for the highlighted item.
 *
 * On entry, caps the selectable quantity at min(gil / price, 99 - owned) and
 * returns -2 when the stack is already full or -1 when no unit is
 * affordable. Afterwards it returns the quantity menu thread state, updating
 * the total-price prompt while the confirmation sub-thread is running. */
static s32 buy_quantity_update(void) {
    s32 owned;
    s32 count;
    s32 limit;

    if (g_shop_quantity_menu_running == 0) {
        g_shop_buy_total_shown = 0;
        g_screen_prompt_active = 1;
        g_shop_buy_item = g_item_list_entries[g_menu_list_selected_index];
        owned = item_total_count_get(g_shop_buy_item);
        if (owned >= 99) {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF807, 0);
            return SHOP_QUANTITY_HOLDING_MAX;
        }
        count = shop_gil_adjust(0);
        g_shop_buy_unit_price = shop_item_price_get(g_shop_buy_item);
        count /= g_shop_buy_unit_price;
        limit = 99 - owned;
        if (limit < count) {
            count = limit;
        }
        if (count == 0) {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF806, 0);
            return SHOP_QUANTITY_CANNOT_AFFORD;
        }
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF808, 0);
        g_shop_quantity_range.bounds.max = count;
        g_shop_buy_total_shown = 0;
        g_sound_suppressed = 1;
    }
    g_shop_quantity_menu_running = menu_managed_thread_start(SCREEN_THREAD_SLOT_UPPER_MENU, g_shop_quantity_menu_data);
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_shop_buy_total_shown == 0) {
        g_text_substitution_values[0]
            = g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY] * g_shop_buy_unit_price;
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF809, 0);
        g_shop_buy_total_shown = 1;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_shop_buy_total_shown != 0) {
        g_shop_buy_total_shown = 0;
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF808, 0);
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
static s32 equip_marker_get(s32 unit_index) {
    formation_unit_t* unit = g_formation_unit_pointers[unit_index];
    s32 i;

    for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
        if (unit->detail.equipment[i] == g_shop_buy_equip_item) {
            return MENU_UNIT_READOUT_EQUIP_LABEL;
        }
    }
    if (equipment_item_compatibility_check(unit_index, g_shop_buy_equip_item) != 1) {
        return MENU_UNIT_READOUT_EQUIP_LABEL;
    }
    return -1;
}

/* Menu callback: sprite brightness for a unit on the shop's "who can equip"
 * view: 0x80 when the unit can equip the candidate item, 0x40 when not. */
static s32 equip_brightness_get(s32 unit_index) {
    return equipment_item_compatibility_check(unit_index, g_shop_buy_equip_item) == 1 ? 0x80 : 0x40;
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
void shop_buy_unit_preview_update(void) {
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
        formation_grid_cursor_init(0);
        g_shop_buy_equip_preview_visible = 0;
        g_shop_buy_equip_initialized = 1;
    }
    g_shop_buy_equip_saved_browse = g_formation_unit_browse_enabled;
    formation_grid_update(0, g_formation_unit_browse_enabled == 0, 1, input, g_menu_input_lock_mode, equip_marker_get,
        equip_brightness_get);
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
                if (g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[i] == g_shop_buy_equip_item) {
                    equipped = 1;
                    g_formation_selected_unit_stat_detail.state.equipment[i] |= FORMATION_HIGHLIGHT;
                }
            }
            if (equipped != 0
                || equipment_item_compatibility_check(g_unit_selected_index, g_shop_buy_equip_item) != 1) {
                g_shop_buy_equip_preview_visible = 0;
            } else {
                slot = formation_item_select_slot(FORMATION_PREVIEW_UNIT_INDEX, g_shop_buy_equip_item);
                formation_item_equip(FORMATION_PREVIEW_UNIT_INDEX, slot, g_shop_buy_equip_item);
                bcopy(g_formation_unit_pointers[FORMATION_PREVIEW_UNIT_INDEX]->detail.equipment,
                    g_item_preview_stat_detail.state.equipment, sizeof(g_item_preview_stat_detail.state.equipment));
                equipment_loadout_delta_calculate(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
                    g_formation_unit_pointers[g_unit_selected_index]->detail.equipment,
                    g_formation_unit_pointers[FORMATION_PREVIEW_UNIT_INDEX]->detail.equipment);
                g_item_preview_stat_detail.state.two_hands = unit_is_two_handing_weapon(
                    g_item_preview_stat_detail.state.equipment, formation_ability_has_two_hands(g_unit_selected_index));
                g_shop_buy_equip_preview_visible = 1;
                g_item_preview_stat_detail.state.equipment[slot] = g_shop_buy_equip_item | FORMATION_HIGHLIGHT;
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
s32 shop_sell_price_get(s32 unused_row) {
    s32 result;

    if (g_menu_list_entry_count == 0) {
        result = 0;
    } else {
        result = shop_item_half_price_get(g_item_list_entries[g_menu_list_selected_index]);
    }
    return result;
}

/* Shop sell list step: on first entry prompts 0xF80B (0xF80C when the
 * player already holds items), rebuilds the item list and installs the seven
 * column callbacks, then handles Cross (back to the main menu) and Circle
 * (sell the highlighted item).
 */
void shop_sell_list_update(void) {
    if (g_shop_sell_list_initialized == 0) {
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, formation_inventory_check(0) != 0 ? 0xF80C : 0xF80B, 0);
        g_shop_item_category = screen_cursor_position_init(SCREEN_CURSOR_SHOP_ITEM_TAB, 0);
        formation_item_list_build(-1, g_shop_item_category + 1,
            g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_scrollable_list_core_init(
            g_item_list_entries, g_menu_list_selected_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[1].value = equipment_get_row_item_count;
        g_menu_row_callbacks[2].icon = item_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
        g_menu_row_callbacks[4].value = shop_sell_price_get;
        g_menu_row_callbacks[5].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[6].value = equipment_get_row_item_count;
        g_shop_sell_list_initialized = 1;
    }
    menu_set_vsync_mode_and_event_speed(2);
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_shop_sell_list_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF802, 0);
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
        g_shop_sell_list_menu_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    shop_gil_draw();
}

/* Item sell confirmation step: pays half price times the quantity, removes the
 * items, prompts 0xF811 and rebuilds the list (0xF80A when nothing to sell).
 */
void shop_sell_confirm_update(void) {
    s32 result = 1;

    if (g_shop_sell_message_wait != 0) {
        if (g_menu_input_primary_repeat
            & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
            g_shop_sell_message_wait = 0;
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80D, 0);
            g_shop_menu_step = SHOP_STEP_SELL_LIST;
            g_screen_prompt_active = 0;
        }
    } else {
        result = sell_quantity_update();
    }
    if (result == 0) {
        if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
            shop_gil_adjust(shop_item_half_price_get(g_item_list_entries[g_menu_list_selected_index])
                * g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            item_inventory_count_adjust(g_item_list_entries[g_menu_list_selected_index],
                -g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY]);
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF811, 0);
            item_rebuild_category_list(g_shop_item_category);
            formation_item_list_build(0, g_shop_item_category + 1,
                g_shop_item_category != ITEM_MENU_TAB_OTHER ? g_shop_item_category + 1 : ITEM_LIST_FILTER_OTHER,
                g_item_list_entries, ITEM_LIST_MODE_ALL);
            screen_list_init(g_item_list_entries, g_menu_list_selected_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], (menu_instruction_t*)g_shop_sell_list_menu_script);
            menu_set_window_scale_step(10);
        } else {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80A, 0);
        }
    }
    if (result <= 0) {
        g_shop_sell_message_wait = 1;
    }
    shop_switch_item_tab(0);
    menu_run_script_with_palette_mode(g_shop_sell_list_menu_script, 0, thread_is_running(THREAD_SCREEN_SLOT_TEXT));
    shop_gil_draw();
}

/* Shop: the quantity prompt of a sale, capped by the inventory count.
 *
 * The first call returns -1 with message 0xF80E when none of the highlighted
 * item is in the inventory. Otherwise it returns the quantity menu's thread
 * state, showing the total (half price times quantity, message 0xF810) while
 * the confirmation child runs and the plain prompt (0xF80F) otherwise. The
 * final frame queues the transaction sound for "yes" and the confirm sound
 * for "no". */
static s32 sell_quantity_update(void) {
    s32 count;

    if (g_shop_sell_result == 0) {
        g_shop_sell_confirm_shown = 0;
        g_shop_sell_item = g_item_list_entries[g_menu_list_selected_index];
        g_screen_prompt_active = 1;
        count = item_inventory_count_adjust(g_shop_sell_item, 0);
        if (count == 0) {
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80E, 0);
            return -1;
        }
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80F, 0);
        g_shop_quantity_range.bounds.max = count;
        g_shop_sell_confirm_shown = 0;
        g_sound_suppressed = 1;
    }
    g_shop_sell_result = menu_managed_thread_start(SCREEN_THREAD_SLOT_UPPER_MENU, g_shop_quantity_menu_data);
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) != 0 && g_shop_sell_confirm_shown == 0) {
        g_text_substitution_values[0]
            = shop_item_half_price_get(g_shop_sell_item) * g_screen_selection_results[SCREEN_SELECTION_SHOP_QUANTITY];
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF810, 0);
        g_shop_sell_confirm_shown = 1;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
    }
    if (thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD) == 0 && g_shop_sell_confirm_shown != 0) {
        g_shop_sell_confirm_shown = 0;
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF80F, 0);
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
