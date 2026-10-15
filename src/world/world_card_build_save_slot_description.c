#include "fft/world.h"
#include "psx/types.h"

/* Build the encoded save-slot description text for WORLD's save screen.
 *
 * mode 1 copies the empty-slot caption; mode 0 formats leader name, job,
 * level, date and location out of the loaded save image. Other nonnegative
 * modes write only the terminator; negative modes leave the buffer untouched.
 *
 * Only the negative-mode exit explicitly returns a value (0). Other paths fall
 * through after writing the terminator, and no caller reads the result.
 * An explicit return at the tail introduces an extra constant load.
 *
 * `world_text_bind_section_data` has no return statement; the job-name table
 * passed on below is the address its store left in $v0. */
s32 world_card_build_save_slot_description(s32 mode, u8* destination) {
    s32 index;
    u8* source;

    if (mode == 1) {
        source = menu_text_find_entry_by_index(g_world_card_slot_text_table, 14, 1);
        while ((*destination++ = *source++) != 0xFE) { }
    } else if (mode < 0) {
        return 0;
    } else if (mode == 0) {
        index = 0;
        while (g_world_load_work_buffer->summary.name[index] != 0xFE) {
            *destination++ = g_world_load_work_buffer->summary.name[index];
            index++;
        }
        *destination++ = 0xE7;
        *destination++ = 0x58;
        source = menu_text_find_entry_by_index(
            world_text_bind_section_data(WORLD_TEXT_DATA_JOB_NAMES), g_world_load_work_buffer->summary.job_id, 1);
        while (*source != 0xFE) {
            *destination++ = *source++;
        }
        *destination++ = 0xE7;
        *destination++ = 0xAC;
        source = menu_text_find_entry_by_index(g_world_card_slot_text_table, 13, 1);
        while (*source != 0xFE) {
            *destination++ = *source++;
        }
        *destination++ = g_world_load_work_buffer->summary.level / 10;
        *destination++ = g_world_load_work_buffer->summary.level % 10;
        *destination++ = 0xFE;
        source = menu_text_find_entry_by_index(
            g_world_card_slot_text_table, g_world_load_work_buffer->summary.date.month, 1);
        while (*source != 0xFE) {
            *destination++ = *source++;
        }
        *destination++ = 0xFA;
        *destination++ = g_world_load_work_buffer->summary.date.day / 10;
        *destination++ = g_world_load_work_buffer->summary.date.day % 10;
        *destination++ = 0xE7;
        *destination++ = 0x58;
        source = menu_text_find_entry_by_index(
            g_text_section_pointers[TEXT_SECTION_LOCATION_NAMES], g_world_load_work_buffer->summary.location, 1);
        while ((*destination++ = *source++) != 0xFE) { }
    }
    *destination = 0xFE;
}
