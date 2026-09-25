#include "fft/event_card.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* An 8-bit count: CARD.OUT stores it with `sb` at 0x801cc814 and reads it
 * with `lb`/`lbu`. This function reads the halfword at that address instead
 * (`lh`), so the read is spelled through an s16 lvalue rather than the
 * declaration. */

/*
 * Memory card menu state 1: choose between loading and saving.
 *
 * g_world_menu_selection_results[5] receives the menu choice (0 load, 1 save, -1 cancel); a card with
 * no occupied slots forces save. Loading when every slot state is -1, or
 * saving when no slot state is 0, shows a notice thread instead of the slot
 * list (the same predicates world_card_count_selectable_save_slots counts). i is pinned to $a0: GCC otherwise swaps it
 * with the flag register.
 */
void world_card_run_mode_select_step(void) {
    u8 previous;
    s32 slot;
    register s32 i __asm__("$4");
    s32 all;

    if (world_card_check_selected() > CARD_IO_RESULT_COMPLETE) {
        g_world_card_pending_message = 0;
        g_card_save_last_written_slot = 0xFF;
        world_menu_stop_thread_and_wait(0xF);
        world_menu_stop_thread_and_wait(6);
        g_world_card_mode_select_running = 0;
        g_world_card_menu_step = 2;
        return;
    }
    if (g_world_card_pending_message != 0) {
        world_menu_start_or_poll_thread(6, g_world_card_pending_message);
        if (g_world_input_primary_repeat & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            g_world_card_pending_message = 0;
            world_menu_stop_thread_and_wait(6);
            g_card_save_last_written_slot = 0xFF;
            g_world_card_menu_step = 0;
        }
        return;
    }
    if (g_world_card_mode_select_running == 0) {
        g_world_menu_selection_results[5] = -1;
        g_world_card_mode_select_menu.cursor = 0;
        g_world_card_mode_select_running = 1;
    }
    if (*(s16*)&g_card_save_occupied_slot_count == 0) {
        g_world_menu_selection_results[5] = 1;
        g_world_card_mode_select_running = 0;
    } else {
        g_world_card_mode_select_running = world_menu_run_thread(0xF, &g_world_card_mode_select_menu);
    }
    if (g_world_card_mode_select_running != 0) {
        return;
    }
    slot = g_world_menu_selection_results[5];
    if (slot == -1) {
        g_world_card_menu_step = 0;
        return;
    }
    i = 0;
    if (slot == 0) {
        for (all = 1; i < 15; i++) {
            if (g_card_save_slot_file_states[i] != -1) {
                all = 0;
            }
        }
        if (all) {
            g_world_card_pending_message = g_world_card_save_unavailable_message;
        }
    } else {
        for (all = 1; i < 15; i++) {
            if (g_card_save_slot_file_states[i] == 0) {
                all = 0;
            }
        }
        if (all) {
            g_world_card_pending_message = g_world_card_no_save_data_message;
        }
    }
    if (g_world_card_pending_message == 0) {
        previous = g_world_card_save_mode;
        g_world_card_save_mode = g_world_menu_selection_results[5];
        g_world_card_menu_step = 3;
        if (previous != g_world_card_save_mode) {
            g_world_card_list_scroll_y = 0x18;
            g_world_card_list_scroll_velocity = 0;
            g_world_card_list_first_visible_row = 0;
            g_world_card_active_slot = 0;
            g_world_card_slot_cursor_row = 0;
        }
        g_world_card_selectable_slot_count = world_card_count_selectable_save_slots();
        return;
    }
    g_world_menu_sound_effect_id = MAIN_SFX_CARD_ERROR;
}
