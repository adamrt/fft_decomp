#include "fft/memory.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Converts a text file's table of 32 section offsets into the absolute section
 * pointers of g_text_section_pointers. The offsets count from the end of
 * the table. */
void text_relocate_pointer_table(const u32* offsets) {
    s32 index;
    const u8* text_data;

    index = 0;
    text_data = (const u8*)offsets + sizeof(u32) * 32;
    do {
        g_text_section_pointers[index] = (u8*)(text_data + offsets[index]);
        index++;
    } while (index < 32);
}

/* Back up the text section pointer table once. */
void text_save_section_pointers(void) {
    if (g_text_section_backup_active == 0) {
        mem_copy_bytes_nonempty(
            g_text_saved_section_pointers, g_text_section_pointers, sizeof(g_text_saved_section_pointers));
        g_text_section_backup_active++;
    }
}

/* Restore the text section pointer table and clear the backup flag. */
void text_restore_section_pointers(void) {
    mem_copy_bytes_nonempty(g_text_section_pointers, g_text_saved_section_pointers, sizeof(g_text_section_pointers));
    g_text_section_backup_active = 0;
}
