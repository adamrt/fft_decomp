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
