#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* An 8-bit count: CARD.OUT stores it with `sb` at 0x801cc814 and reads it
 * with `lb`/`lbu`. This function reads the 16-bit value at that address instead
 * (`lh`), so the read is spelled through an s16 lvalue rather than the
 * declaration. */

/*
 * Memory card menu state 1: choose between loading and saving.
 *
 * g_world_menu_selection_results[5] receives the menu choice (0 save, 1 load, -1 cancel); a zero
 * count (the title's Continue passes 0) skips the menu and loads. Saving when every slot state is -1,
 * or loading when no slot state is 0, shows a notice thread instead of the slot
 * list (the same predicates world_card_count_selectable_save_slots counts). `i` is reused to hold the
 * previous save mode.
 */
void world_card_run_mode_select_step(void) {
    s32 slot;
    s32 i;
    s32 all;

    if (world_card_check_selected() > CARD_IO_RESULT_COMPLETE) {
        g_world_card_pending_message = 0;
        g_main_card_save_last_used_slot = CARD_SAVE_LAST_USED_SLOT_NONE;
        menu_stop_thread_and_wait(WORLD_THREAD_SLOT_UPPER_MENU);
        menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
        g_world_card_mode_select_running = 0;
        g_world_card_menu_step = WORLD_CARD_MENU_STEP_SCANNING;
        return;
    }
    if (g_world_card_pending_message != 0) {
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, g_world_card_pending_message);
        if (g_menu_input_primary_repeat & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            g_world_card_pending_message = 0;
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_main_card_save_last_used_slot = CARD_SAVE_LAST_USED_SLOT_NONE;
            g_world_card_menu_step = WORLD_CARD_MENU_STEP_SELECT_CARD;
        }
        return;
    }
    if (g_world_card_mode_select_running == 0) {
        g_world_menu_selection_results[5] = -1;
        g_world_card_mode_select_menu.selected_index = 0;
        g_world_card_mode_select_running = 1;
    }
    if (g_world_card_mode_select_enabled == 0) {
        g_world_menu_selection_results[5] = 1;
        g_world_card_mode_select_running = 0;
    } else {
        g_world_card_mode_select_running
            = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_card_mode_select_menu);
    }
    if (g_world_card_mode_select_running != 0) {
        return;
    }
    slot = g_world_menu_selection_results[5];
    if (slot == -1) {
        g_world_card_menu_step = WORLD_CARD_MENU_STEP_SELECT_CARD;
        return;
    }
    if (slot == 0) {
        for (i = 0, all = 1; i < 15; i++) {
            if (g_main_card_save_slot_file_states[i] != CARD_SAVE_SLOT_UNAVAILABLE) {
                all = 0;
            }
        }
        if (all) {
            g_world_card_pending_message = &g_world_card_save_unavailable_message;
        }
    } else {
        for (i = 0, all = 1; i < 15; i++) {
            if (g_main_card_save_slot_file_states[i] == CARD_SAVE_SLOT_OCCUPIED) {
                all = 0;
            }
        }
        if (all) {
            g_world_card_pending_message = &g_world_card_no_save_data_message;
        }
    }
    if (g_world_card_pending_message == 0) {
        i = g_world_card_save_mode;
        g_world_card_save_mode = g_world_menu_selection_results[5];
        g_world_card_menu_step = WORLD_CARD_MENU_STEP_SELECT_SAVE;
        if (i != g_world_card_save_mode) {
            g_world_card_list_scroll_y = 0x18;
            g_world_card_list_scroll_velocity = 0;
            g_world_card_list_first_visible_row = 0;
            g_world_card_active_slot = 0;
            g_world_card_slot_cursor_row = 0;
        }
        g_world_card_selectable_slot_count = world_card_count_selectable_save_slots();
        return;
    }
    g_sound_queued_effect_id = MAIN_SFX_ERROR_MESSAGE;
}
