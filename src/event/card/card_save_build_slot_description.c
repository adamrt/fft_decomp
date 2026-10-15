#include "fft/event_card.h"
#include "fft/menu.h"
#include "psx/types.h"

/*
 * Compose the two-line text of a save slot into `destination`.
 *
 * Mode 1 copies text entry 14 (the empty-slot line). Mode 0 builds the line
 * for the save buffer's header: name, job name at x 0x58, entry 13 (the
 * level label) at x 0xac with the two level digits, end of line; then the
 * month name, a space, the two day digits and the location name at x 0x58.
 * Digits are stored as glyphs 0..9 and each line ends with 0xfe.
 */
void card_save_build_slot_description(s32 mode, u8* destination) {
    s32 index;
    u8 stack_padding[4];
    const u8* source;

    if (mode == 1) {
        source = menu_text_find_entry_by_index(g_card_text_data, 14, 1);
        while ((*destination++ = *source++) != 0xfe) { }
    } else if (mode == 0) {
        index = 0;
        while (g_card_save_buffer_pointer->summary.name[index] != 0xfe) {
            *destination++ = g_card_save_buffer_pointer->summary.name[index];
            index++;
        }

        *destination++ = 0xe7;
        *destination++ = 0x58;
        source = menu_text_find_entry_by_index(g_card_text_job_name, g_card_save_buffer_pointer->summary.job_id, 1);
        while (*source != 0xfe) {
            *destination++ = *source++;
        }

        *destination++ = 0xe7;
        *destination++ = 0xac;
        source = menu_text_find_entry_by_index(g_card_text_data, 13, 1);
        while (*source != 0xfe) {
            *destination++ = *source++;
        }

        *destination++ = g_card_save_buffer_pointer->summary.level / 10;
        *destination++ = g_card_save_buffer_pointer->summary.level % 10;
        *destination++ = 0xfe;
        source = menu_text_find_entry_by_index(g_card_text_data, g_card_save_buffer_pointer->summary.date.month, 1);
        while (*source != 0xfe) {
            *destination++ = *source++;
        }

        *destination++ = 0xfa;
        *destination++ = g_card_save_buffer_pointer->summary.date.day / 10;
        *destination++ = g_card_save_buffer_pointer->summary.date.day % 10;
        *destination++ = 0xe7;
        *destination++ = 0x58;
        source
            = menu_text_find_entry_by_index(g_card_text_location_name, g_card_save_buffer_pointer->summary.location, 1);
        while ((*destination++ = *source++) != 0xfe) { }
    }
    *destination = 0xfe;
}
