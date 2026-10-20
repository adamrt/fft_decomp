/* The fitting room previews equipment, tracks the bill and commits or undoes the chosen loadout. */

#include "fft/equipment.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/formation_item.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/item_menu.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/message_box.h"
#include "fft/screen.h"
#include "fft/shop.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_shop_background_visible;

/* Result of checkout_prompt_update. */
typedef enum checkout_result {
    SHOP_FITTING_CHECKOUT_DONE = -1,    /* bought, or left without buying: back to the shop's main menu */
    SHOP_FITTING_CHECKOUT_CANCELED = 0, /* back to the fitting room */
    SHOP_FITTING_CHECKOUT_PROMPT_OPEN = 1
} checkout_result_e;
extern u8 g_shop_fitting_initialized;
extern u8 g_shop_fitting_cost_visible;
extern s32 g_shop_fitting_cost;
extern u8 g_shop_fitting_unit_chosen;
extern s8 g_shop_fitting_item_list_open;
extern s16 g_shop_fitting_previewed_cursor;
extern s8 g_shop_fitting_item_list_close;
extern s8 g_shop_fitting_equip_menu_initialized;
extern s8 g_shop_fitting_equip_slot;
extern u16 g_shop_fitting_slot_cursor_anim;
extern point16_t g_shop_fitting_slot_cursor_point;
extern s8 g_shop_fitting_remove_preview_initialized;
extern u16 g_shop_fitting_remove_cursor_anim;
extern point16_t g_shop_fitting_remove_cursor_point;
extern menu_entry_t g_shop_fitting_sell_equipped_menu;
extern u8 g_shop_fitting_sell_equipped_menu_open;
extern u16 g_shop_fitting_sell_equipped_cursor_anim;
extern point16_t g_shop_fitting_sell_equipped_cursor_point;
extern u8 g_shop_fitting_sell_equipped_prompt_active;
extern u8 g_shop_fitting_sell_equipped_slot;
extern u8 g_shop_fitting_sell_equipped_result_shown;
extern u8 g_shop_fitting_sell_initialized;
extern s16 g_shop_fitting_sell_step; /* shop_step_e: SELL_LIST or SELL_CONFIRM */

extern menu_entry_t g_shop_fitting_checkout_confirm_menu;
extern menu_entry_t g_shop_fitting_checkout_short_gil_menu;
extern u8 g_shop_fitting_checkout_prompt_initialized;
extern menu_entry_t* g_shop_fitting_checkout_prompt_menu;
extern u8 g_shop_fitting_checkout_prompt_active;
extern u8 g_shop_fitting_checkout_prompt_result;
extern u8 g_shop_fitting_checkout_complete;
extern u8 g_shop_fitting_checkout_insufficient_gil;

/* Two-entry menu chain: row 3 of [0] opens [1] through [0]'s parent-index table. */
extern menu_entry_t g_shop_fitting_menu_thread_data[2];

/* Last equipment slot whose stat preview was built, -1 when none. The
 * ATTACK overlay has an unrelated 16-bit value at this same address. */
extern s8 g_shop_fitting_remove_previewed_equip_slot;
extern s16 g_shop_fitting_remove_previewed_unit_index;

/* The best-fit confirmation menu entry, followed directly by its open/closed
 * gate byte, as g_shop_fitting_sell_equipped_menu / _open are paired. */
extern menu_entry_t g_shop_fitting_best_fit_menu;
extern s8 g_shop_fitting_best_fit_menu_open;
extern u8 g_shop_fitting_item_list_menu_script[213];
extern u8 g_shop_fitting_cost_menu_script[103];

static void fitting_cost_draw(void);
static s32 checkout_prompt_update(void);
static void fitting_undo(void);
static void trial_item_return(s16 unit_index, s16 equipment_index);
static void trial_item_remove(s16 formation_index, s16 equipment_index);
static void fitting_purchase_commit(void);

/* Menu callback: add `delta` to the fitting room's running cost and return
 * the new total. */
static s32 fitting_cost_add(s32 delta) {
    return g_shop_fitting_cost = delta + g_shop_fitting_cost;
}

/* Menu callback: no marker (-1) for any unit. */
static s32 fitting_unit_marker_get(s32 unused_unit_index) {
    return -1;
}

/* Fitting-room menu step: on entry (after a button press) clear the trial equipment and its cost;
 * then either browse the formation (Circle or Triangle opens the fitting menu for the unit, Cross
 * goes to the checkout) or run the fitting menu thread and dispatch its selection to the next
 * shop step. */
void shop_fitting_menu_update(void) {
    s32 busy;

    if (g_shop_fitting_initialized == 0) {
        if (g_menu_input_newly_pressed == 0) {
            shop_gil_draw();
            return;
        }
        formation_grid_cursor_init(0);
        memset(g_shop_fitting_items, 0, sizeof(g_shop_fitting_items));
        /* Subtract the running cost from itself: back to 0. */
        fitting_cost_add(-fitting_cost_add(0));
        g_shop_fitting_unit_chosen = 0;
        g_shop_fitting_cost_visible = 0;
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
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
        busy = screen_input_lockout_get();
        busy += formation_grid_scroll_check();
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
            menu_selection_records_clear();
        } else {
            formation_grid_update(
                1, 1, 0, g_menu_input_secondary_repeat, g_menu_input_lock_mode, fitting_unit_marker_get, 0);
        }
    } else {
        g_shop_background_visible = 1;
        g_help_message_text_id = 0;
        if (menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, g_shop_fitting_menu_thread_data) == 0) {
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
                formation_selection_cursor_init(g_unit_selected_index);
                g_formation_unit_cycle_mode = 0;
                g_formation_unit_browse_enabled = 0;
                g_shop_fitting_cost_visible = 0;
                if (g_screen_selection_results[1] == 4) {
                    g_formation_unit_banner_enabled = 0;
                    g_shop_menu_step = SHOP_STEP_FITTING_ROOM_CHECKOUT;
                }
            }
            menu_selection_value_init(SCREEN_CURSOR_REMOVE_SLOT, 0);
            menu_selection_value_init(SCREEN_CURSOR_EQUIP_SLOT, 0);
        }
    }
    if (g_shop_fitting_cost_visible != 0) {
        fitting_cost_draw();
    }
}

/* Shop fitting-room equipment step, the shop twin of equipment_menu_update.
 *
 * With the slot list active, steps the equipment-slot cursor and shows the
 * equipped item's description; confirming opens the shop item list for the
 * slot. In the item list, confirming an equippable item returns the unit's
 * current item to the fitting room, equips the shop item (bit 15 marks it as
 * unpaid), adds its price to the fitting-room cost and recalculates stats,
 * while moving the cursor previews the stat change. Cancel leaves the list,
 * or from the slot list goes back to the fitting room's unit grid. */
void shop_fitting_equipment_update(void) {
    s16 selected_index;
    s16 scroll_base_index;
    s32 prev;
    s32 slot;
    s32 value;
    u16 item;
    s16 equip;
    s32 result;

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
        g_shop_fitting_equip_slot = menu_selection_vertical_sound_step(
            MENU_EQUIPMENT_SLOT_COUNT, SCREEN_CURSOR_EQUIP_SLOT, g_menu_input_primary_repeat, SOUND_SFX_PAGE_SWITCH);
        if (prev != g_shop_fitting_equip_slot) {
            g_menu_list_selected_index = 0;
        }

        if (g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[g_shop_fitting_equip_slot]
            != ITEM_ID_NOTHING) {
            value = (g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[g_shop_fitting_equip_slot]
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
            shop_stock_list_build(g_unit_selected_index, g_screen_context_id, g_shop_fitting_equip_slot,
                g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);

            /* Both hand slots share selection record 0, so the record is the slot less one. */
            slot = 0;
            if (g_shop_fitting_equip_slot != 0) {
                slot = g_shop_fitting_equip_slot - 1;
            }
            menu_selection_record_restore(slot, &selected_index, &scroll_base_index, g_item_list_entries);
            menu_init_scrollable_list(g_item_list_entries, selected_index, scroll_base_index,
                g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
            g_menu_row_callbacks[1].value = equipment_get_row_item_count;
            g_menu_row_callbacks[2].icon = item_build_type_icon_source;
            g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
            g_menu_row_callbacks[4].value = shop_buy_price_get;
            menu_set_vsync_mode_and_event_speed(2);
        }
    } else {
        item = g_item_list_entries[g_menu_list_selected_index];

        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0 || (item & ITEM_ENTRY_UNEQUIPPABLE)) {
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
            } else {
                equip = item;
                result = formation_item_slot_check(g_unit_selected_index, g_shop_fitting_equip_slot, equip);
                if (result < 0) {
                    g_sound_queued_effect_id = SOUND_SFX_INVALID;
                } else {
                    trial_item_return(g_unit_selected_index, g_shop_fitting_equip_slot);
                    g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[g_shop_fitting_equip_slot]
                        = item | SHOP_FITTING_ITEM;
                    fitting_cost_add(shop_item_price_get(equip));
                    /* 0: the item clashes with the other hand, which goes back to the fitting room too. */
                    if (result != 1) {
                        trial_item_return(g_unit_selected_index, g_shop_fitting_equip_slot == 0);
                    }
                    g_shop_fitting_item_list_close = 1;
                    g_sound_queued_effect_id = SOUND_SFX_EQUIP;
                    formation_selected_stats_refresh();
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
                equipment_loadout_delta_calculate(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
                    g_formation_unit_pointers[g_unit_selected_index]->detail.equipment,
                    g_formation_temp_unit->detail.equipment);
                bcopy(g_formation_unit_pointers[g_unit_selected_index]->detail.equipment,
                    g_formation_temp_unit->detail.equipment, sizeof(g_formation_temp_unit->detail.equipment));
            } else {
                formation_toggle_preview_stats_window(0);
            }
            g_unit_panel_status_display_thread_params.redraw_request = 1;
        }

        menu_run_script_with_palette_mode(
            g_shop_fitting_item_list_menu_script, g_menu_input_primary_repeat, screen_busy_check());
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
        menu_selection_record_set(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
    }

    g_shop_fitting_slot_cursor_point.y = g_shop_fitting_equip_slot * 16 + 0x90;
    gfx_set_otag_index(0x3d);
    screen_sliding_cursor_draw(&g_shop_fitting_slot_cursor_point, &g_shop_fitting_slot_cursor_anim,
        g_shop_fitting_item_list_open != 0 || g_menu_input_lock_mode != 0);
    if (g_shop_fitting_cost_visible != 0) {
        fitting_cost_draw();
    }
}

/* Row callback: the war funds left after paying for the items being tried on; the row is ignored. */
static s32 remaining_gil_get(s32 unused_row) {
    s32 gil = shop_gil_adjust(0);

    return gil - fitting_cost_add(0);
}

/* Report whether menu input is locked or the screen menu slot's thread still runs.
 *
 * Its only caller passes 0, but the body never reads that argument: `$a0` is
 * overwritten with 6 before any use and the result starts from zero. */
static s32 fitting_menu_busy_check(s32 unused) {
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
static void fitting_cost_draw(void) {
    g_menu_row_callbacks[0].value = shop_gil_adjust;
    g_menu_row_callbacks[1].value = fitting_cost_add;
    g_menu_row_callbacks[2].value = remaining_gil_get;
    menu_run_script_with_palette_mode(
        g_shop_fitting_cost_menu_script, g_menu_input_primary_repeat, fitting_menu_busy_check(0));
}

/* Fitting-room counterpart of unequip_menu_update: takes one equipment
 * slot off the selected unit, refunding a shop item's price from the fitting
 * room cost, and previews the stat change of the slot under the cursor
 * against the unit's own item held in the fitting room.
 *
 * The prologue's $a0 is not an argument register here. The callee,
 * formation_selection_stage_update, takes no arguments; $a0 simply holds the copy of the selected-unit index that
 * the store to g_shop_fitting_remove_previewed_unit_index consumes, while the compare keeps the load in $v1. A
 * one-argument prototype would pull the load pseudo into $a0 and push the copy into $v1; pins and launders cannot
 * compensate, because the target's copy sits in the beq's delay slot and a tied asm is never a delay-slot candidate. */
void shop_fitting_remove_update(void) {
    /* The target reserves 64 local bytes whose contents are not accessed;
     * saved registers sit at 0x60..0x70 under a -0x78 frame. */
    s32 unused[16];
    s8 slot;
    s32 index;
    s32 value;
    s32 equipped_item;
    s16 unit;

    if (g_shop_fitting_remove_previewed_unit_index != g_unit_selected_index) {
        unit = g_unit_selected_index;
        g_shop_fitting_remove_previewed_unit_index = unit;
        g_shop_fitting_remove_preview_initialized = 0;
        formation_selection_stage_update();
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
    slot = menu_selection_vertical_sound_step(
        MENU_EQUIPMENT_SLOT_COUNT, SCREEN_CURSOR_REMOVE_SLOT, g_menu_input_primary_repeat, SOUND_SFX_PAGE_SWITCH);
    if (g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[(u8)slot] != ITEM_ID_NOTHING) {
        value = (g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[(u8)slot] & ITEM_ENTRY_ID_MASK)
            + HELP_TEXT_ID_ITEM_BASE;
    } else {
        value = -1;
    }
    g_help_message_text_id = value;
    g_shop_fitting_remove_cursor_point.y = (u8)slot * 16 + 0x90;
    gfx_set_otag_index(0x3D);
    screen_sliding_cursor_draw(
        &g_shop_fitting_remove_cursor_point, &g_shop_fitting_remove_cursor_anim, menu_input_lock_mode_get(0));
    /* Only a tried-on shop item (bit 15) can be taken off here; its price comes off the cost. */
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        equipped_item = g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[(u8)slot];
        if ((equipped_item >> 15) == 0) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        } else {
            fitting_cost_add(-shop_item_price_get(equipped_item & ITEM_ENTRY_ID_MASK));
            trial_item_remove(g_unit_selected_index, slot & 0xFF);
            formation_selected_stats_refresh();
            g_sound_queued_effect_id = SOUND_SFX_UNEQUIP;
            g_shop_fitting_remove_previewed_equip_slot = -1;
        }
    }
    index = (u8)slot;
    if (g_shop_fitting_remove_previewed_equip_slot != index) {
        equipped_item = g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[index];
        g_shop_fitting_remove_previewed_equip_slot = slot;
        if ((equipped_item >> 15) != 0) {
            formation_toggle_preview_stats_window(1);
            equipment_swap_delta_calculate(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
                equipped_item, g_shop_fitting_items[g_unit_selected_index][index], index);
            g_unit_panel_preview_stats_thread_params.redraw_request = 1;
        } else {
            formation_toggle_preview_stats_window(0);
        }
        g_unit_panel_status_display_thread_params.redraw_request = 1;
    }
    if (g_shop_fitting_cost_visible != 0) {
        fitting_cost_draw();
    }
}

/*
 * Fitting room step that prices the tried-on equipment.
 *
 * Runs the checkout prompt: done returns to the shop's main menu (message
 * 0xf802); canceled shows message 0xf818 until any face button, which
 * returns to the fitting room with the unit's stats recalculated.
 */
void shop_fitting_checkout_update(void) {
    s32 result;

    g_shop_background_visible = 1;
    if (g_shop_fitting_checkout_complete != 0) {
        if (g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
            g_shop_fitting_checkout_complete = 0;
            g_formation_unit_banner_enabled = 1;
            formation_selected_stats_refresh();
        }
    } else {
        result = checkout_prompt_update();
        if (result == SHOP_FITTING_CHECKOUT_DONE) {
            g_shop_menu_step = SHOP_STEP_MAIN_MENU;
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf802, 0);
            g_shop_fitting_initialized = 0;
        } else if (result == SHOP_FITTING_CHECKOUT_CANCELED) {
            g_shop_fitting_checkout_complete = 1;
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf818, 0);
        }
    }
    shop_gil_draw();
}

/*
 * Run one frame of the fitting-room checkout prompt.
 *
 * Opens the pay prompt when the party can afford the fitting-room cost and
 * the "not enough gil" prompt otherwise; an empty cart is bought
 * immediately. Returns 1 while a prompt is open; 0 and -1 are the two
 * closing outcomes shop_fitting_checkout_update dispatches on.
 */
static s32 checkout_prompt_update(void) {
    s32 balance;
    s32 cost;

    if (g_shop_fitting_checkout_prompt_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
            return SHOP_FITTING_CHECKOUT_PROMPT_OPEN;
        }
        g_shop_fitting_checkout_confirm_menu.selected_index = 0;
        g_shop_fitting_checkout_short_gil_menu.selected_index = 0;
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        balance = shop_gil_adjust(0) - (cost = fitting_cost_add(0));
        if (cost == 0) {
            fitting_purchase_commit();
            return SHOP_FITTING_CHECKOUT_DONE;
        }
        if (balance >= 0) {
            g_sound_suppressed = 1;
            g_shop_fitting_checkout_prompt_menu = &g_shop_fitting_checkout_confirm_menu;
            g_text_substitution_values[0] = cost;
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf819, 0);
            g_shop_fitting_checkout_insufficient_gil = 0;
        } else {
            g_shop_fitting_checkout_prompt_menu = &g_shop_fitting_checkout_short_gil_menu;
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf81a, 0);
            g_shop_fitting_checkout_insufficient_gil = 1;
        }
        g_shop_fitting_checkout_prompt_initialized = 1;
        g_shop_fitting_checkout_prompt_active = 1;
    }
    if (g_shop_fitting_checkout_prompt_active != 0) {
        g_shop_fitting_checkout_prompt_active
            = menu_managed_thread_start(SCREEN_THREAD_SLOT_UPPER_MENU, g_shop_fitting_checkout_prompt_menu);
        if (g_shop_fitting_checkout_prompt_active != 0) {
            return SHOP_FITTING_CHECKOUT_PROMPT_OPEN;
        }
        g_shop_fitting_checkout_prompt_result = g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT];
        if (g_shop_fitting_checkout_insufficient_gil == 0) {
            g_sound_suppressed = 0;
            if (g_shop_fitting_checkout_prompt_result == 0) {
                balance = -fitting_cost_add(0);
                shop_gil_adjust(balance);
                fitting_cost_add(balance);
                fitting_purchase_commit();
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
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf81c, 0);
            return SHOP_FITTING_CHECKOUT_PROMPT_OPEN;
        }
        if (g_shop_fitting_checkout_prompt_result != 1) {
            fitting_undo();
            g_shop_fitting_checkout_prompt_initialized = 0;
            return SHOP_FITTING_CHECKOUT_DONE;
        }
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_shop_fitting_checkout_prompt_initialized = 0;
        return SHOP_FITTING_CHECKOUT_CANCELED;
    }
    if (menu_managed_thread_start(SCREEN_THREAD_SLOT_UPPER_MENU, &g_shop_fitting_checkout_short_gil_menu) != 0) {
        return SHOP_FITTING_CHECKOUT_PROMPT_OPEN;
    }
    g_shop_fitting_checkout_prompt_initialized = 0;
    if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] != 1) {
        if (g_shop_fitting_checkout_prompt_result != 0) {
            fitting_undo();
            return SHOP_FITTING_CHECKOUT_DONE;
        }
        return SHOP_FITTING_CHECKOUT_DONE;
    }
    thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
    return SHOP_FITTING_CHECKOUT_CANCELED;
}

/* Undo the fitting room: give every unit back the items it set aside there and clear the
 * reservations. A slot holding a tried-on shop item (bit 14 or 15 set) is restored even when
 * nothing was set aside, which empties it. */
static void fitting_undo(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_formation_unit_count; i++) {
        for (j = 0; j < MENU_EQUIPMENT_SLOT_COUNT; j++) {
            if ((g_formation_unit_pointers[i]->detail.equipment[j] >> 14) != 0
                || g_shop_fitting_items[i][j] != ITEM_ID_NOTHING) {
                g_formation_unit_pointers[i]->detail.equipment[j] = g_shop_fitting_items[i][j];
            }
            g_shop_fitting_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}

/*
 * Shop "best fit / fitting room" preview step, entry 8
 * (SHOP_STEP_FITTING_ROOM_BEST_FIT) of g_shop_step_handlers.
 *
 * On entry it stages the selected unit's four weapon/armor slots, fills any
 * slot already taken from the shop with that slot's fitting-room reservation,
 * asks shop_fitting_best_fit_build for a suggested set,
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
 * The target has `move s1,v0` after menu_managed_thread_start and no
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
void shop_fitting_best_fit_update(void) {
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
        bcopy(g_formation_unit_pointers[g_unit_selected_index]->detail.equipment, staged, sizeof(staged));
        unit_slot = &g_formation_unit_pointers[g_unit_selected_index];
        room_base = &g_shop_fitting_items[0][0];
        reserved = room_base + g_unit_selected_index * MENU_EQUIPMENT_SLOT_COUNT;
        for (i = 0; i < MENU_EQUIPMENT_SLOT_ACCESSORY; i++, reserved++) {
            if (((*unit_slot)->detail.equipment[i] >> 15) != 0) {
                (*unit_slot)->detail.equipment[i] = *reserved;
            }
        }
        shop_fitting_best_fit_build(
            g_unit_selected_index, g_screen_context_id, g_item_preview_stat_detail.state.equipment);
        g_item_preview_stat_detail.state.equipment[MENU_EQUIPMENT_SLOT_ACCESSORY]
            = g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[MENU_EQUIPMENT_SLOT_ACCESSORY];
        bcopy(staged, g_formation_unit_pointers[g_unit_selected_index]->detail.equipment, sizeof(staged));
        equipment_loadout_delta_calculate(&g_item_preview_stat_detail, &g_unit_panel_selected_unit_stat_summary,
            g_formation_unit_pointers[g_unit_selected_index]->detail.equipment,
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
    i = menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, &g_shop_fitting_best_fit_menu);
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
                 * register is sign-extended again for shop_item_price_get. */
                suggested = g_item_preview_stat_detail.state.equipment[i];
                shifted = suggested << 16;
                item = shifted >> 16;
                current = g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[i];
                if (item == 0 || shifted < 0) {
                    trial_item_return(g_unit_selected_index, i);
                    fitting_cost_add(shop_item_price_get(item));
                } else if ((current >> 15) != 0) {
                    fitting_cost_add(-shop_item_price_get(current));
                    g_shop_fitting_items[g_unit_selected_index][i] = ITEM_ID_NOTHING;
                }
                g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[i] = item;
            }
            formation_selected_stats_refresh();
        }
        g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
        g_sound_suppressed = 0;
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
    }
    if (g_shop_fitting_cost_visible != 0) {
        fitting_cost_draw();
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
void shop_fitting_equipped_sale_update(void) {
    s32 close = 0;
    u32 item;
    s32 text_id;
    u32 input;

    if (g_shop_fitting_sell_equipped_menu_open == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80c, 0);
            g_shop_fitting_sell_equipped_prompt_active = 0;
            g_formation_unit_cycle_mode = 1;
            g_shop_fitting_sell_equipped_menu_open = 1;
            g_shop_fitting_sell_equipped_result_shown = 0;
        }
    } else {
        if (g_shop_fitting_sell_equipped_result_shown != 0) {
            if (g_menu_input_primary_repeat
                & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_DPAD_MASK)) {
                thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80d, 0);
                g_shop_fitting_sell_equipped_result_shown = 0;
            }
        } else if (g_shop_fitting_sell_equipped_prompt_active == 0) {
            g_shop_fitting_sell_equipped_slot = menu_selection_vertical_sound_step(MENU_EQUIPMENT_SLOT_COUNT,
                SCREEN_CURSOR_REMOVE_SLOT, g_menu_input_primary_repeat, SOUND_SFX_PAGE_SWITCH);
            if (g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[g_shop_fitting_sell_equipped_slot]
                != ITEM_ID_NOTHING) {
                text_id = (g_formation_unit_pointers[g_unit_selected_index]
                                  ->detail.equipment[g_shop_fitting_sell_equipped_slot]
                              & ITEM_ENTRY_ID_MASK)
                    + HELP_TEXT_ID_ITEM_BASE;
            } else {
                text_id = -1;
            }
            input = g_menu_input_primary_repeat;
            g_help_message_text_id = text_id;
            if (input & PSX_PAD_CIRCLE) {
                item = g_formation_unit_pointers[g_unit_selected_index]
                           ->detail.equipment[g_shop_fitting_sell_equipped_slot];
                if (item == ITEM_ID_NOTHING || (item >> 15) != 0) {
                    g_sound_queued_effect_id = SOUND_SFX_INVALID;
                } else {
                    g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                    g_shop_fitting_sell_equipped_prompt_active = 1;
                    g_shop_fitting_sell_equipped_menu.selected_index = 0;
                    g_text_substitution_values[0] = shop_item_half_price_get(item);
                    thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf810, 0);
                }
            } else if (input & PSX_PAD_CROSS) {
                close = 1;
            }
        } else {
            g_help_message_text_id = 0;
            g_sound_suppressed = g_shop_fitting_sell_equipped_prompt_active
                = menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, &g_shop_fitting_sell_equipped_menu);
            if (g_shop_fitting_sell_equipped_prompt_active == 0) {
                if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 0) {
                    thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf811, 0);
                    shop_gil_adjust(g_text_substitution_values[0]);
                    g_formation_unit_pointers[g_unit_selected_index]
                        ->detail.equipment[g_shop_fitting_sell_equipped_slot] = ITEM_ID_NOTHING;
                    formation_selected_stats_refresh();
                    g_sound_queued_effect_id = SOUND_SFX_SHOP_TRANSACTION;
                } else {
                    if (g_screen_selection_results[SCREEN_SELECTION_PROMPT_RESULT] == 1) {
                        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                    }
                    thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf80a, 0);
                }
                g_shop_fitting_sell_equipped_result_shown = 1;
            }
        }
        if (close != 0) {
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_formation_unit_cycle_mode = 2;
            g_shop_fitting_sell_equipped_menu_open = 0;
            g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
            thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        }
        g_shop_fitting_sell_equipped_cursor_point.y = g_shop_fitting_sell_equipped_slot * 16 + 0x90;
        gfx_set_otag_index(0x3d);
        screen_sliding_cursor_draw(&g_shop_fitting_sell_equipped_cursor_point,
            &g_shop_fitting_sell_equipped_cursor_anim, menu_input_lock_mode_get(0));
    }
    fitting_cost_draw();
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
void shop_fitting_inventory_sale_update(void) {
    if (g_shop_fitting_sell_initialized == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
            return;
        }
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
        g_formation_unit_browse_enabled = 0;
        g_formation_unit_banner_enabled = 0;
        formation_panels_stop();
        g_formation_unit_cycle_mode = 0;
        g_shop_fitting_sell_step = SHOP_STEP_SELL_LIST;
        menu_selection_record_clear(5);
        menu_selection_record_clear(6);
        menu_selection_record_clear(7);
        menu_selection_record_clear(8);
        menu_selection_record_clear(9);
        g_shop_fitting_sell_initialized = 1;
    }
    g_shop_menu_step = g_shop_fitting_sell_step;
    if (g_shop_fitting_sell_step == SHOP_STEP_SELL_LIST) {
        shop_sell_list_update();
        if (g_shop_fitting_sell_step != g_shop_menu_step) {
            screen_selection_results_reset();
        }
    } else if (g_shop_fitting_sell_step == SHOP_STEP_SELL_CONFIRM) {
        shop_sell_confirm_update();
    }
    if (g_shop_menu_step == SHOP_STEP_MAIN_MENU) {
        g_shop_menu_step = SHOP_STEP_FITTING_ROOM;
        g_formation_unit_cycle_mode = 2;
        g_shop_fitting_sell_initialized = 0;
        g_formation_unit_browse_enabled = 1;
        menu_set_vsync_mode_and_event_speed(2);
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        return;
    }
    g_shop_fitting_sell_step = g_shop_menu_step;
    g_shop_menu_step = SHOP_STEP_FITTING_ROOM_SELL;
}

/* Clears one equipment slot of a formation unit.  An item the unit owned goes
 * back to the fitting room reserve; an item taken from the shop (bit 15 set)
 * is refunded from the fitting room cost instead. */
static void trial_item_return(s16 unit_index, s16 equipment_index) {
    s32 equipped_item = g_formation_unit_pointers[unit_index]->detail.equipment[equipment_index];

    if (equipped_item != ITEM_ID_NOTHING) {
        if ((equipped_item >> 15) == 0)
            g_shop_fitting_items[unit_index][equipment_index] = equipped_item;
        else
            fitting_cost_add(-shop_item_price_get(equipped_item & ITEM_ENTRY_ID_MASK));
        g_formation_unit_pointers[unit_index]->detail.equipment[equipment_index] = ITEM_ID_NOTHING;
    }
}

/* Take the tried-on item off one slot and put back what the unit set aside in the fitting room:
 * for a hand slot, both hands' reserved items when both hands are now empty, else the slot's
 * own reserved item if it fits beside the other hand; any other slot always gets its reserved
 * item (possibly nothing) back. The fit check uses g_unit_selected_index, which is what every
 * caller passes as formation_index. */
static void trial_item_remove(s16 formation_index, s16 equipment_index) {
    u16 reserved_item;

    g_formation_unit_pointers[formation_index]->detail.equipment[equipment_index] = ITEM_ID_NOTHING;
    reserved_item = g_shop_fitting_items[formation_index][equipment_index];
    if (equipment_index < MENU_EQUIPMENT_SLOT_HEAD) {
        u16 right_hand = g_formation_unit_pointers[formation_index]->detail.equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
        u16 left_hand = g_formation_unit_pointers[formation_index]->detail.equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND];

        if (right_hand == ITEM_ID_NOTHING && left_hand == ITEM_ID_NOTHING) {
            g_formation_unit_pointers[formation_index]->detail.equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND]
                = g_shop_fitting_items[formation_index][MENU_EQUIPMENT_SLOT_RIGHT_HAND];
            g_formation_unit_pointers[formation_index]->detail.equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND]
                = g_shop_fitting_items[formation_index][MENU_EQUIPMENT_SLOT_LEFT_HAND];
            g_shop_fitting_items[formation_index][MENU_EQUIPMENT_SLOT_RIGHT_HAND] = ITEM_ID_NOTHING;
            g_shop_fitting_items[formation_index][MENU_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
            return;
        }
        if (reserved_item != ITEM_ID_NOTHING
            && formation_item_slot_check(g_unit_selected_index, equipment_index, reserved_item) == 1) {
            g_formation_unit_pointers[formation_index]->detail.equipment[equipment_index] = reserved_item;
            g_shop_fitting_items[formation_index][equipment_index] = ITEM_ID_NOTHING;
        }
    } else {
        g_formation_unit_pointers[formation_index]->detail.equipment[equipment_index] = reserved_item;
        g_shop_fitting_items[formation_index][equipment_index] = ITEM_ID_NOTHING;
    }
}

/* Complete a fitting-room purchase: every tried-on item (entry bits 14-15
 * set) becomes the unit's own, and the equipment it displaced, kept in
 * g_shop_fitting_items, goes back to the inventory. */
static void fitting_purchase_commit(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_formation_unit_count; i++) {
        for (j = 0; j < MENU_EQUIPMENT_SLOT_COUNT; j++) {
            s32 equipped_item = g_formation_unit_pointers[i]->detail.equipment[j];
            if ((equipped_item >> 14) != 0) {
                g_formation_unit_pointers[i]->detail.equipment[j] &= ITEM_ENTRY_ID_MASK;
            }
            item_inventory_count_adjust(g_shop_fitting_items[i][j], 1);
            g_shop_fitting_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}
