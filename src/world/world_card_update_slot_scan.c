#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
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
        if (thread_is_running(THREAD_SCREEN_SLOT_MENU) == 0) {
            g_world_card_slot_scan_active = 0;
            g_world_card_menu_step = WORLD_CARD_MENU_STEP_SELECT_CARD;
            g_world_card_error_message_closing = 0;
        }
        return;
    }

    if (g_world_card_format_delay_counter != 0) {
        g_world_card_format_delay_counter++;
        if (g_world_card_format_delay_counter == 3) {
            world_menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_world_card_slot_scan_active = 0;
            g_world_card_format_delay_counter = 0;
            world_card_format_selected_slot();
            g_world_card_menu_step = WORLD_CARD_MENU_STEP_SELECT_CARD;
        }
        return;
    }

    if (g_world_card_slot_scan_active == 0) {
        world_input_clear_state();
        if (g_main_card_save_last_used_slot < 0x10) {
            if (world_card_wait_for_selected_status(5) == CARD_IO_RESULT_COMPLETE) {
                g_world_card_slot_scan_index = 0;
                while (g_world_card_slot_scan_index < CARD_SAVE_SLOT_COUNT) {
                    if (g_main_card_save_slot_file_states[g_world_card_slot_scan_index] >= 0) {
                        bcopy(&g_main_card_save_slot_metadata[g_world_card_slot_scan_index],
                            &g_world_load_work_buffer->summary, sizeof(card_save_summary_t));
                        world_card_build_save_slot_description(
                            g_main_card_save_slot_file_states[g_world_card_slot_scan_index],
                            g_world_card_save_slot_descriptions[g_world_card_slot_scan_index]);
                        world_card_draw_slot_text(g_world_card_slot_scan_index);
                    }
                    g_world_card_slot_scan_index++;
                }
                g_world_card_menu_step = WORLD_CARD_MENU_STEP_SELECT_MODE;
                return;
            } else {
                g_main_card_save_last_used_slot = CARD_SAVE_LAST_USED_SLOT_NONE;
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
                world_menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_world_card_checking_message);
            }
            g_world_card_scan_start_frame++;
            return;
        }

        g_world_card_probe_result = world_card_probe_selected();
        if (g_world_card_probe_result == CARD_IO_RESULT_NEW_CARD) {
            g_world_card_probe_result = world_card_probe_selected();
        }
        if (g_world_card_probe_result == CARD_IO_RESULT_COMPLETE) {
            g_world_card_save_file_count = world_card_count_save_files(g_world_card_directory_buffer, 1);
            result = 0;
            while (result < 15) {
                status = world_card_count_save_files(g_world_card_directory_buffer, 1);
                if (status == g_world_card_save_file_count) {
                    break;
                } else {
                    g_world_card_save_file_count = status;
                }
                VSync(2);
                result++;
            }
            g_world_card_free_block_count
                = world_card_count_free_blocks(g_world_card_directory_buffer, g_world_card_save_file_count);
        } else {
            world_menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_world_menu_sound_effect_id = MAIN_SFX_ERROR_MESSAGE;
        }

        for (result = 0; result < 15; result++) {
            g_main_card_save_slot_file_states[result] = CARD_SAVE_SLOT_UNSCANNED;
        }
        g_world_menu_selection_results[5] = -1;
        g_world_card_format_prompt[1].selected_index = 1;
        g_main_card_save_last_used_slot = CARD_SAVE_LAST_USED_SLOT_NONE;
        g_world_card_scan_prompt_active = 1;
        g_world_card_slot_scan_index = 0;
        g_world_card_slot_scan_active = 1;
        g_world_card_scan_start_frame = 0;
        return;
    }

    if (g_world_card_probe_result == CARD_IO_RESULT_NEW_CARD) {
        if (world_card_check_selected() > CARD_IO_RESULT_COMPLETE) {
            world_menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            world_menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU_CHILD);
            g_world_card_slot_scan_active = 0;
        } else if (g_world_card_scan_prompt_active == 0) {
            if (g_world_menu_selection_results[5] == 0) {
                g_world_card_format_delay_counter = 1;
            } else {
                g_world_card_menu_step = WORLD_CARD_MENU_STEP_SELECT_CARD;
            }
            g_world_card_slot_scan_active = 0;
        } else {
            g_world_card_scan_prompt_active
                = world_menu_run_thread(THREAD_SCREEN_SLOT_MENU, g_world_card_format_prompt);
            if (g_world_card_scan_prompt_active == 0 && g_world_menu_selection_results[5] == 0) {
                world_menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_world_card_formatting_message);
            }
        }
        return;
    }

    if (g_world_card_probe_result == CARD_IO_RESULT_ERROR || g_world_card_probe_result == CARD_IO_RESULT_TIMEOUT) {
        result = world_card_check_selected();
        world_menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_world_card_access_error_message);
        if ((g_world_input_primary_repeat & PSX_PAD_CROSS) != 0
            || (g_world_input_primary_repeat & PSX_PAD_CIRCLE) != 0) {
            world_menu_stop_thread(THREAD_SCREEN_SLOT_MENU);
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
            g_world_card_error_message_closing = 1;
        } else if (result == CARD_IO_RESULT_NEW_CARD || result == CARD_IO_RESULT_COMPLETE) {
            world_menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
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
                    g_world_card_directory_buffer[entry].name);
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
                    world_menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
                    return;
                }
                if (descriptor != -1) {
                    status = world_card_seek_file_with_retries(descriptor, 0x100, 0);
                    if (status != -1) {
                        status = card_file_read_with_retries(descriptor, &g_world_load_work_buffer->summary, 0x80);
                    }
                    if (status != -1 && g_world_load_work_buffer->summary.slot >= 0x10
                        && world_card_delete_file(g_world_card_save_file_names[g_world_card_slot_scan_index]) != 0) {
                        g_world_card_free_block_count++;
                        status = 0;
                    }
                    if (card_file_close_with_retries(descriptor) == 0) {
                        status = -1;
                    }
                    if (status == -1) {
                        g_world_card_slot_scan_active = 0;
                        world_menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
                        return;
                    }
                } else if (descriptor == -1) {
                    status = 0;
                }
            }

            if (status == 0x80) {
                g_main_card_save_slot_file_states[g_world_card_slot_scan_index] = CARD_SAVE_SLOT_OCCUPIED;
            }
            if (status <= 0) {
                if (g_world_card_free_block_count > 0) {
                    g_main_card_save_slot_file_states[g_world_card_slot_scan_index] = CARD_SAVE_SLOT_AVAILABLE;
                    g_world_card_free_block_count--;
                } else {
                    g_main_card_save_slot_file_states[g_world_card_slot_scan_index] = CARD_SAVE_SLOT_UNAVAILABLE;
                }
            }
            if (g_main_card_save_slot_file_states[g_world_card_slot_scan_index] >= 0) {
                world_card_build_save_slot_description(g_main_card_save_slot_file_states[g_world_card_slot_scan_index],
                    g_world_card_save_slot_descriptions[g_world_card_slot_scan_index]);
                bcopy(&g_world_load_work_buffer->summary, &g_main_card_save_slot_metadata[g_world_card_slot_scan_index],
                    sizeof(card_save_summary_t));
                world_card_update_save_slot_playtime(g_world_card_slot_scan_index);
                world_card_draw_slot_text(g_world_card_slot_scan_index);
            }
            g_world_card_slot_scan_index++;
        } else {
            g_main_card_save_last_used_slot = 0;
            world_menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_world_card_menu_step = WORLD_CARD_MENU_STEP_SELECT_MODE;
            g_world_card_slot_scan_active = 0;
        }
    }
}
