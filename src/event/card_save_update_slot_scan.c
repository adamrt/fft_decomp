#include "fft/event_card.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

enum {
    CARD_LAST_WRITTEN_SAVE_SLOT_NONE = 0xff,
};

/* Dispatched in scanning state by card_save_run_menu. */
void card_save_update_slot_scan(void) {
    s32 result;
    s32 status;
    s32 unused_18;
    s32 unused_1c;
    s32 unused_20;
    s32 unused_24;
    s32 descriptor;
    s32 unused_2c;
    s32 unused_30;
    s32 unused_34;
    RECT rect;

    if (g_card_save_error_prompt_closing != 0) {
        if (battle_thread_is_running(6) == 0) {
            g_card_save_slot_scan_active = 0;
            g_card_save_menu_state = CARD_SAVE_MENU_SELECT_CARD;
            g_card_save_error_prompt_closing = 0;
        }
        return;
    }

    if (g_card_save_format_delay_counter != 0) {
        g_card_save_format_delay_counter++;
        if (g_card_save_format_delay_counter == 3) {
            card_thread_wait_and_clear_state(6);
            g_card_save_slot_scan_active = 0;
            g_card_save_format_delay_counter = 0;
            card_io_format_selected_slot();
            g_card_save_menu_state = CARD_SAVE_MENU_SELECT_CARD;
        }
        return;
    }

    if (g_card_save_slot_scan_active == 0) {
        card_input_clear_state();
        if (g_card_save_last_written_slot < 0x10) {
            result = card_io_wait_operation_result(5);
            if (result == CARD_IO_RESULT_COMPLETE) {
                g_card_save_slot_scan_index = 0;
                while (g_card_save_slot_scan_index < CARD_SAVE_SLOT_COUNT) {
                    if (g_card_save_slot_file_states[g_card_save_slot_scan_index] >= 0) {
                        bcopy(g_card_save_slot_metadata[g_card_save_slot_scan_index], &g_card_save_buffer_pointer->slot,
                            0x18);
                        card_save_build_slot_description(g_card_save_slot_file_states[g_card_save_slot_scan_index],
                            g_card_save_slot_descriptions[g_card_save_slot_scan_index].data);
                        card_save_render_slot_text(g_card_save_slot_scan_index);
                    }
                    g_card_save_slot_scan_index++;
                }
                g_card_save_menu_state = CARD_SAVE_MENU_SCAN_RESULT;
                return;
            } else {
                g_card_save_last_written_slot = CARD_LAST_WRITTEN_SAVE_SLOT_NONE;
            }
        }

        if (g_card_save_scan_setup_step < 3) {
            if (g_card_save_scan_setup_step == 0) {
                rect.x = 0x180;
                rect.y = 0;
                rect.w = 0x40;
                rect.h = 0xf0;
                ClearImage(&rect, 0, 0, 0);
            }
            if (g_card_save_scan_setup_step == 1) {
                card_thread_start_managed_with_delay(6, g_card_save_checking_message_descriptor);
            }
            g_card_save_scan_setup_step++;
            return;
        }

        g_card_io_open_result = card_io_open_or_create_selected_card();
        if (g_card_io_open_result == CARD_IO_RESULT_COMPLETE) {
            g_card_save_file_count = card_file_enumerate_saves(g_card_save_menu_work_buffer, 1);
            result = 0;
            while (result < 15) {
                status = card_file_enumerate_saves(g_card_save_menu_work_buffer, 1);
                if (status == g_card_save_file_count) {
                    break;
                } else {
                    g_card_save_file_count = status;
                }
                VSync(2);
                result++;
            }
            g_card_free_block_count = card_file_count_free_blocks(g_card_save_menu_work_buffer, g_card_save_file_count);
        } else {
            card_thread_wait_and_clear_state(6);
            g_card_sound_queued_effect_id = MAIN_SFX_CARD_ERROR;
        }

        result = 0;
        while (result < 15) {
            g_card_save_slot_file_states[result] = CARD_SAVE_SLOT_UNSCANNED;
            result++;
        }
        g_card_save_slot_index = -1;
        g_card_save_format_prompt_descriptor[1].selected_index = 1;
        g_card_save_last_written_slot = CARD_LAST_WRITTEN_SAVE_SLOT_NONE;
        g_card_save_format_prompt_active = 1;
        g_card_save_slot_scan_index = 0;
        g_card_save_slot_scan_active = 1;
        g_card_save_scan_setup_step = 0;
        return;
    }

    if (g_card_io_open_result == CARD_IO_RESULT_NEW_CARD) {
        result = card_io_poll_operation_result();
        if (result > CARD_IO_RESULT_COMPLETE) {
            card_thread_wait_and_clear_state(6);
            card_thread_wait_and_clear_state(5);
            g_card_save_slot_scan_active = 0;
        } else if (g_card_save_format_prompt_active == 0) {
            if (g_card_save_slot_index == 0) {
                g_card_save_format_delay_counter = 1;
            } else {
                g_card_save_menu_state = CARD_SAVE_MENU_SELECT_CARD;
            }
            g_card_save_slot_scan_active = 0;
        } else {
            g_card_save_format_prompt_active = card_thread_start_managed(6, g_card_save_format_prompt_descriptor);
            if (g_card_save_format_prompt_active == 0 && g_card_save_slot_index == 0) {
                card_thread_start_managed_with_delay(6, g_card_save_formatting_message_descriptor);
            }
        }
        return;
    }

    if (g_card_io_open_result == CARD_IO_RESULT_ERROR || g_card_io_open_result == CARD_IO_RESULT_TIMEOUT) {
        result = card_io_poll_operation_result();
        card_thread_start_managed_with_delay(6, g_card_save_card_error_prompt_descriptor);
        if ((g_card_input_primary_repeat & PSX_PAD_CROSS) != 0 || (g_card_input_primary_repeat & PSX_PAD_CIRCLE) != 0) {
            card_thread_stop_and_clear_state(6);
            g_card_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_card_save_error_prompt_closing = 1;
        } else if (result == CARD_IO_RESULT_NEW_CARD || result == CARD_IO_RESULT_COMPLETE) {
            card_thread_wait_and_clear_state(6);
            g_card_save_slot_scan_active = 0;
        }
        return;
    }

    if (g_card_io_open_result == CARD_IO_RESULT_COMPLETE) {
        card_input_clear_state();
        if (g_card_save_slot_scan_index < CARD_SAVE_SLOT_COUNT) {
            status = 1;
            result = 0;
            while (result < g_card_save_file_count) {
                status = strcmp(g_card_save_file_names[g_card_save_slot_scan_index],
                    (const char*)g_card_save_menu_work_buffer[result].name);
                if (status == 0) {
                    break;
                }
                result++;
            }
            status = status ? -1 : 0;

            if (status != -1) {
                memset(g_card_save_buffer_pointer, 0xff, 0x1e80);
                descriptor
                    = card_file_open_selected_with_retries(g_card_save_file_names[g_card_save_slot_scan_index], 1);
                if (descriptor == -2) {
                    g_card_save_slot_scan_active = 0;
                    card_thread_wait_and_clear_state(6);
                    return;
                }
                if (descriptor != -1) {
                    status = card_file_seek_with_retries(descriptor, 0x100, 0);
                    if (status != -1) {
                        status = card_file_read_with_retries(descriptor, &g_card_save_buffer_pointer->slot, 0x80);
                    }
                    if (status != -1 && g_card_save_buffer_pointer->slot >= 0x10
                        && card_file_delete_selected_with_retries(g_card_save_file_names[g_card_save_slot_scan_index])
                            != 0) {
                        g_card_free_block_count++;
                        status = 0;
                    }
                    if (card_file_close_with_retries(descriptor) == 0) {
                        status = -1;
                    }
                    if (status == -1) {
                        g_card_save_slot_scan_active = 0;
                        card_thread_wait_and_clear_state(6);
                        return;
                    }
                } else if (descriptor == -1) {
                    status = 0;
                }
            }

            if (status == 0x80) {
                g_card_save_slot_file_states[g_card_save_slot_scan_index] = CARD_SAVE_SLOT_OCCUPIED;
            }
            if (status <= 0) {
                if (g_card_free_block_count > 0) {
                    g_card_save_slot_file_states[g_card_save_slot_scan_index] = CARD_SAVE_SLOT_AVAILABLE;
                    g_card_free_block_count--;
                } else {
                    g_card_save_slot_file_states[g_card_save_slot_scan_index] = CARD_SAVE_SLOT_UNAVAILABLE;
                }
            }
            if (g_card_save_slot_file_states[g_card_save_slot_scan_index] >= 0) {
                card_save_build_slot_description(g_card_save_slot_file_states[g_card_save_slot_scan_index],
                    g_card_save_slot_descriptions[g_card_save_slot_scan_index].data);
                bcopy(&g_card_save_buffer_pointer->slot, g_card_save_slot_metadata[g_card_save_slot_scan_index], 0x18);
                card_save_update_slot_playtime(g_card_save_slot_scan_index);
                card_save_render_slot_text(g_card_save_slot_scan_index);
            }
            g_card_save_slot_scan_index++;
        } else {
            g_card_save_last_written_slot = 0;
            card_thread_wait_and_clear_state(6);
            g_card_save_menu_state = CARD_SAVE_MENU_SCAN_RESULT;
            g_card_save_slot_scan_active = 0;
        }
    }
}
