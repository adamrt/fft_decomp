#include "fft/card.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/etc.h"
#include "psx/gpu.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Scans the selected memory card and classifies each of the 15 save slots.
 *
 * WORLD twin of CARD's card_save_update_slot_scan (0x801c0c58): same state
 * machine, but a NEW_CARD probe result is retried once before use.
 */
void world_card_update_slot_scan(void) {
    s32 result;
    s32 status;
    s32 descriptor;
    s32 entry;
    RECT rect;

    if (g_world_card_error_message_closing != 0) {
        if (world_thread_is_running(6) == 0) {
            g_world_card_slot_scan_active = 0;
            g_world_card_menu_step = 0;
            g_world_card_error_message_closing = 0;
        }
        return;
    }

    if (g_world_card_format_delay_counter != 0) {
        g_world_card_format_delay_counter++;
        if (g_world_card_format_delay_counter == 3) {
            world_menu_stop_thread_and_wait(6);
            g_world_card_slot_scan_active = 0;
            g_world_card_format_delay_counter = 0;
            world_card_format_selected_slot();
            g_world_card_menu_step = 0;
        }
        return;
    }

    if (g_world_card_slot_scan_active == 0) {
        world_input_clear_state();
        if (g_card_save_last_written_slot < 0x10) {
            if (world_card_wait_for_selected_status(5) == CARD_IO_RESULT_COMPLETE) {
                g_world_card_slot_scan_index = 0;
                while (g_world_card_slot_scan_index < CARD_SAVE_SLOT_COUNT) {
                    if (g_card_save_slot_file_states[g_world_card_slot_scan_index] >= 0) {
                        bcopy(g_card_save_slot_metadata[g_world_card_slot_scan_index], &g_world_load_work_buffer->slot,
                            0x18);
                        world_card_build_save_slot_description(
                            g_card_save_slot_file_states[g_world_card_slot_scan_index],
                            g_world_card_save_slot_descriptions[g_world_card_slot_scan_index]);
                        world_card_draw_slot_text(g_world_card_slot_scan_index);
                    }
                    g_world_card_slot_scan_index++;
                }
                g_world_card_menu_step = 1;
                return;
            } else {
                g_card_save_last_written_slot = 0xff;
            }
        }

        if (g_world_card_scan_start_frame < 3) {
            if (g_world_card_scan_start_frame == 0) {
                rect.x = 0x180;
                rect.y = 0;
                rect.w = 0x40;
                rect.h = 0xf0;
                ClearImage(&rect, 0, 0, 0);
            }
            if (g_world_card_scan_start_frame == 1) {
                world_menu_start_or_poll_thread(6, g_world_card_checking_message);
            }
            g_world_card_scan_start_frame++;
            return;
        }

        g_world_card_probe_result = world_card_probe_selected();
        if (g_world_card_probe_result == CARD_IO_RESULT_NEW_CARD) {
            g_world_card_probe_result = world_card_probe_selected();
        }
        if (g_world_card_probe_result == CARD_IO_RESULT_COMPLETE) {
            g_world_card_save_file_count = world_card_count_save_files((DIRENTRY*)g_world_card_directory_buffer, 1);
            result = 0;
            while (result < 15) {
                status = world_card_count_save_files((DIRENTRY*)g_world_card_directory_buffer, 1);
                if (status == g_world_card_save_file_count) {
                    break;
                } else {
                    g_world_card_save_file_count = status;
                }
                VSync(2);
                result++;
            }
            g_world_card_free_block_count
                = world_card_count_free_blocks((DIRENTRY*)g_world_card_directory_buffer, g_world_card_save_file_count);
        } else {
            world_menu_stop_thread_and_wait(6);
            g_world_menu_sound_effect_id = MAIN_SFX_CARD_ERROR;
        }

        for (result = 0; result < 15; result++) {
            g_card_save_slot_file_states[result] = CARD_SAVE_SLOT_UNSCANNED;
        }
        g_world_menu_selection_results[5] = -1;
        g_world_card_format_prompt[0].selection = 1;
        g_card_save_last_written_slot = 0xff;
        g_world_card_scan_prompt_active = 1;
        g_world_card_slot_scan_index = 0;
        g_world_card_slot_scan_active = 1;
        g_world_card_scan_start_frame = 0;
        return;
    }

    if (g_world_card_probe_result == CARD_IO_RESULT_NEW_CARD) {
        if (world_card_check_selected() > CARD_IO_RESULT_COMPLETE) {
            world_menu_stop_thread_and_wait(6);
            world_menu_stop_thread_and_wait(5);
            g_world_card_slot_scan_active = 0;
        } else if (g_world_card_scan_prompt_active == 0) {
            if (g_world_menu_selection_results[5] == 0) {
                g_world_card_format_delay_counter = 1;
            } else {
                g_world_card_menu_step = 0;
            }
            g_world_card_slot_scan_active = 0;
        } else {
            g_world_card_scan_prompt_active = world_menu_run_thread(6, g_world_card_format_prompt);
            if (g_world_card_scan_prompt_active == 0 && g_world_menu_selection_results[5] == 0) {
                world_menu_start_or_poll_thread(6, g_world_card_formatting_message);
            }
        }
        return;
    }

    if (g_world_card_probe_result == CARD_IO_RESULT_ERROR || g_world_card_probe_result == CARD_IO_RESULT_TIMEOUT) {
        result = world_card_check_selected();
        world_menu_start_or_poll_thread(6, g_world_card_access_error_message);
        if ((g_world_input_primary_repeat & PSX_PAD_CROSS) != 0
            || (g_world_input_primary_repeat & PSX_PAD_CIRCLE) != 0) {
            world_menu_stop_thread(6);
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
            g_world_card_error_message_closing = 1;
        } else if (result == CARD_IO_RESULT_NEW_CARD || result == CARD_IO_RESULT_COMPLETE) {
            world_menu_stop_thread_and_wait(6);
            g_world_card_slot_scan_active = 0;
        }
        return;
    }

    if (g_world_card_probe_result == CARD_IO_RESULT_COMPLETE) {
        world_input_clear_state();
        if (g_world_card_slot_scan_index < CARD_SAVE_SLOT_COUNT) {
            status = 1;
            entry = 0;
            while (entry < g_world_card_save_file_count) {
                status = strcmp(g_world_card_save_file_names[g_world_card_slot_scan_index],
                    (const char*)((DIRENTRY*)g_world_card_directory_buffer)[entry].name);
                if (status == 0) {
                    break;
                }
                entry++;
            }
            status = status ? -1 : 0;

            if (status != -1) {
                memset(g_world_load_work_buffer, 0xff, 0x1e80);
                descriptor
                    = world_card_open_file_with_retries(g_world_card_save_file_names[g_world_card_slot_scan_index], 1);
                if (descriptor == -2) {
                    g_world_card_slot_scan_active = 0;
                    world_menu_stop_thread_and_wait(6);
                    return;
                }
                if (descriptor != -1) {
                    status = world_card_seek_file_with_retries(descriptor, 0x100, 0);
                    if (status != -1) {
                        status = world_card_read_file_with_retries(descriptor, &g_world_load_work_buffer->slot, 0x80);
                    }
                    if (status != -1 && g_world_load_work_buffer->slot >= 0x10
                        && world_card_delete_file(g_world_card_save_file_names[g_world_card_slot_scan_index]) != 0) {
                        g_world_card_free_block_count++;
                        status = 0;
                    }
                    if (world_card_close_file_with_retries(descriptor) == 0) {
                        status = -1;
                    }
                    if (status == -1) {
                        g_world_card_slot_scan_active = 0;
                        world_menu_stop_thread_and_wait(6);
                        return;
                    }
                } else if (descriptor == -1) {
                    status = 0;
                }
            }

            if (status == 0x80) {
                g_card_save_slot_file_states[g_world_card_slot_scan_index] = CARD_SAVE_SLOT_OCCUPIED;
            }
            if (status <= 0) {
                if (g_world_card_free_block_count > 0) {
                    g_card_save_slot_file_states[g_world_card_slot_scan_index] = CARD_SAVE_SLOT_AVAILABLE;
                    g_world_card_free_block_count--;
                } else {
                    g_card_save_slot_file_states[g_world_card_slot_scan_index] = CARD_SAVE_SLOT_UNAVAILABLE;
                }
            }
            if (g_card_save_slot_file_states[g_world_card_slot_scan_index] >= 0) {
                world_card_build_save_slot_description(g_card_save_slot_file_states[g_world_card_slot_scan_index],
                    g_world_card_save_slot_descriptions[g_world_card_slot_scan_index]);
                bcopy(&g_world_load_work_buffer->slot, g_card_save_slot_metadata[g_world_card_slot_scan_index], 0x18);
                world_card_update_save_slot_playtime(g_world_card_slot_scan_index);
                world_card_draw_slot_text(g_world_card_slot_scan_index);
            }
            g_world_card_slot_scan_index++;
        } else {
            g_card_save_last_written_slot = 0;
            world_menu_stop_thread_and_wait(6);
            g_world_card_menu_step = 1;
            g_world_card_slot_scan_active = 0;
        }
    }
}
