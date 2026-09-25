#include "fft/event_card.h"
#include "psx/pad.h"
#include "psx/types.h"

enum {
    CARD_LAST_WRITTEN_SAVE_SLOT_NONE = 0xff,
};

void card_save_update_slot_write(s32 slot) {
    s32 write_result;
    s32 header_result;
    s32 unused_18;
    s32 descriptor;
    s32 unused_20;
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
        card_thread_start_managed_with_delay(6, g_card_save_in_progress_thread_descriptor);
        g_battle_sound_suppressed = 1;
        return;
    } else {
        if (g_card_save_write_phase < 2) {
            g_card_save_write_phase++;
            return;
        }
    }

    g_battle_sound_suppressed = 0;
    if (g_card_save_write_failed != 0) {
        card_thread_start_managed_with_delay(6, g_card_save_failure_thread_descriptor);
        if ((g_card_input_primary_repeat & PSX_PAD_CIRCLE) != 0 || (g_card_input_primary_repeat & PSX_PAD_CROSS) != 0) {
            card_thread_wait_and_clear_state(6);
            g_card_save_write_phase = 0;
            g_card_save_menu_state = CARD_SAVE_MENU_SCANNING;
        }
        return;
    }

    write_result = card_file_write_buffer(g_card_save_file_names[slot], (const u8*)g_card_save_buffer_pointer, 0x1e00,
        g_card_save_slot_file_states[slot]);
    card_thread_wait_and_clear_state(6);
    header_result = 0;
    if (write_result == 0x1e00) {
        g_card_save_buffer_pointer->slot = slot;
        descriptor = card_file_open_selected_with_retries(g_card_save_file_names[slot], 2);
        if (descriptor >= 0) {
            header_result = card_file_seek_with_retries(descriptor, 0x100, 0);
            if (header_result != -1) {
                header_result = card_file_write_with_retries(descriptor, &g_card_save_buffer_pointer->slot, 0x80);
            }
            if (card_file_close_with_retries(descriptor) == 0) {
                header_result = 0;
            }
        }
    }

    if (write_result == 0x1e00 && header_result == 0x80) {
        g_card_save_menu_state = CARD_SAVE_MENU_SELECT_SAVE;
        g_card_save_slot_file_states[slot] = 0;
        card_save_build_slot_description(0, g_card_save_slot_descriptions[slot].data);
        card_save_render_slot_text(slot);
        bcopy(&g_card_save_buffer_pointer->slot, g_card_save_slot_metadata[slot], 0x18);
        card_save_update_slot_playtime(slot);
        g_card_sound_queued_effect_id = 0x85;
        g_card_save_write_phase = 0;
        g_card_save_completed = 1;
        g_card_save_last_written_slot = slot;
        return;
    }

    g_card_save_last_written_slot = CARD_LAST_WRITTEN_SAVE_SLOT_NONE;
    g_card_save_write_failed = 1;
    g_card_sound_queued_effect_id = MAIN_SFX_CARD_ERROR;
}
