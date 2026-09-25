#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Advance the selected save slot's write operation and error prompt.
 *
 * The 0x1E00-byte save image is written first; on success the slot number
 * is stamped into the header, and the 0x80-byte block at +0x100 is
 * rewritten in place. A complete write refreshes the slot's cached
 * metadata and graphic, otherwise the error prompt is shown.
 */
void world_card_run_save_step(s32 slot) {
    u32 state = g_world_card_save_step_state;
    s32 input;
    s32 bytes_written;
    s32 header_written;
    s32 descriptor;
    s32 unused[6]; /* unreferenced locals: the target frame is 0x40 */

    if (state == 0) {
        g_world_card_save_step_state = 1;
        g_world_card_save_failed = 0;
        D_801CC7D8 = 0;
        world_card_build_save_image(slot);
        world_menu_start_or_poll_thread(6, g_world_card_saving_message);
        return;
    }
    if (state < 2) {
        g_world_card_save_step_state = state + 1;
        return;
    }
    if (g_world_card_save_failed != 0) {
        world_menu_start_or_poll_thread(6, g_world_card_save_error_message);
        input = g_world_input_primary_repeat;
        if ((input & PSX_PAD_CIRCLE) || (input & PSX_PAD_CROSS)) {
            world_menu_stop_thread_and_wait(6);
            g_world_card_save_step_state = 0;
            g_world_card_menu_step = 2;
        }
    } else {
        bytes_written = world_card_write_buffer_to_file(
            g_world_card_save_file_names[slot], g_world_load_work_buffer, 0x1E00, g_card_save_slot_file_states[slot]);
        header_written = 0;
        world_menu_stop_thread_and_wait(6);
        if (bytes_written == 0x1E00) {
            g_world_load_work_buffer->slot = slot;
            descriptor = world_card_open_file_with_retries(g_world_card_save_file_names[slot], 2);
            if (descriptor >= 0) {
                header_written = world_card_seek_file_with_retries(descriptor, 0x100, 0);
                if (header_written != -1) {
                    header_written
                        = world_card_write_file_with_retries(descriptor, &g_world_load_work_buffer->slot, 0x80);
                }
                if (world_card_close_file_with_retries(descriptor) == 0) {
                    header_written = 0;
                }
            }
            if (bytes_written == 0x1E00 && header_written == 0x80) {
                g_world_card_menu_step = 3;
                g_card_save_slot_file_states[slot] = 0;
                world_card_build_save_slot_description(0, g_world_card_save_slot_descriptions[slot]);
                world_card_draw_slot_text(slot);
                bcopy(&g_world_load_work_buffer->slot, g_card_save_slot_metadata[slot], 0x18);
                world_card_update_save_slot_playtime(slot);
                g_world_menu_sound_effect_id = 0x85;
                g_world_card_save_step_state = 0;
                g_card_save_last_written_slot = slot;
                return;
            }
        }
        g_card_save_last_written_slot = 0xFF;
        g_world_card_save_failed = 1;
        g_world_menu_sound_effect_id = MAIN_SFX_CARD_ERROR;
    }
}
