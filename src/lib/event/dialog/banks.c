/* Bind text-bank section offsets and preserve the current section pointers while an
 * overlay temporarily supplies another bank. */

#include "fft/memory.h"
#include "fft/text.h"
#include "psx/types.h"

extern u8* g_text_saved_section_pointers[TEXT_SECTION_COUNT];
extern s32 g_text_section_backup_active;

/* Bind writable sections of a text bank; Formation rebuilds the roster-name section in place.
 * The bank starts with TEXT_SECTION_COUNT offsets measured from the end of that table. */
void text_pointer_table_relocate(u32* offsets) {
    s32 index = 0;
    u8* text_data = (u8*)offsets + sizeof(u32) * TEXT_SECTION_COUNT;

    do {
        g_text_section_pointers[index] = text_data + offsets[index];
        index++;
    } while (index < TEXT_SECTION_COUNT);
}

/* Back up the text section pointer table once. */
void text_section_pointers_save(void) {
    if (g_text_section_backup_active == 0) {
        mem_copy_bytes_nonempty(
            g_text_saved_section_pointers, g_text_section_pointers, sizeof(g_text_saved_section_pointers));
        g_text_section_backup_active++;
    }
}

/* Restore the text section pointer table and clear the backup flag. */
void text_section_pointers_restore(void) {
    mem_copy_bytes_nonempty(g_text_section_pointers, g_text_saved_section_pointers, sizeof(g_text_section_pointers));
    g_text_section_backup_active = 0;
}
