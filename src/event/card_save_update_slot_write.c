#include "fft/event_card.h"
#include "fft/sound.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Save-menu state CARD_SAVE_MENU_WRITING: writes the game to save slot `slot`.
 *
 * The first call builds the save buffer and shows the "saving" notice; the
 * write itself happens two calls later so the notice is on screen. The
 * 0x1e00-byte file is written in one go, then reopened to write the 0x80-byte
 * slot header at offset 0x100. On success the slot is marked occupied, its
 * description, list text and play time are refreshed and it becomes the last
 * used slot. On failure the error notice stays until Circle or Cross, which
 * returns the menu to the card scan.
 */
void card_save_update_slot_write(s32 slot) {
    s32 write_result;
    s32 header_result;
    s32 unused_18;
    s32 descriptor;
    s32 unused_24;
    s32 unused_28;
    s32 unused_2c;
    s32 unused_30;
    s32 unused_34;
    s32 unused_38;
    s32 unused_3c;

    if (g_card_save_write_phase == 0) {
        g_card_save_write_phase = 1;
        g_card_save_write_failed = 0;
        D_801ca964 = 0;
        card_save_build_buffer(slot);
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, g_card_save_in_progress_thread_descriptor);
        g_sound_suppressed = 1;
        return;
    } else {
        if (g_card_save_write_phase < 2) {
            g_card_save_write_phase++;
            return;
        }
    }

    g_sound_suppressed = 0;
    if (g_card_save_write_failed != 0) {
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, g_card_save_failure_thread_descriptor);
        if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) != 0 || (g_menu_input_primary_repeat & PSX_PAD_CROSS) != 0) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_card_save_write_phase = 0;
            g_card_save_menu_state = CARD_SAVE_MENU_SCANNING;
        }
        return;
    }

    write_result = card_file_write_buffer(
        g_card_save_file_names[slot], g_card_save_buffer_pointer, 0x1e00, g_main_card_save_slot_file_states[slot]);
    menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
    header_result = 0;
    if (write_result == 0x1e00) {
        g_card_save_buffer_pointer->summary.slot = slot;
        descriptor = card_file_open_selected_with_retries(g_card_save_file_names[slot], 2);
        if (descriptor >= 0) {
            header_result = card_file_seek_with_retries(descriptor, 0x100, 0);
            if (header_result != -1) {
                header_result = card_file_write_with_retries(descriptor, &g_card_save_buffer_pointer->summary, 0x80);
            }
            if (card_file_close_with_retries(descriptor) == 0) {
                header_result = 0;
            }
        }
    }

    if (write_result == 0x1e00 && header_result == 0x80) {
        g_card_save_menu_state = CARD_SAVE_MENU_SELECT_SAVE;
        g_main_card_save_slot_file_states[slot] = CARD_SAVE_SLOT_OCCUPIED;
        card_save_build_slot_description(0, g_card_save_slot_descriptions[slot]);
        card_save_render_slot_text(slot);
        bcopy(&g_card_save_buffer_pointer->summary, &g_main_card_save_slot_metadata[slot], sizeof(card_save_summary_t));
        card_save_update_slot_playtime(slot);
        g_card_sound_queued_effect_id = MAIN_SFX_COMPLETE;
        g_card_save_write_phase = 0;
        g_card_save_completed = 1;
        g_main_card_save_last_used_slot = slot;
        return;
    }

    g_main_card_save_last_used_slot = CARD_SAVE_LAST_USED_SLOT_NONE;
    g_card_save_write_failed = 1;
    g_card_sound_queued_effect_id = MAIN_SFX_ERROR_MESSAGE;
}
