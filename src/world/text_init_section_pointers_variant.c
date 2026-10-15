#include "fft/world.h"
#include "psx/types.h"

/* Turn the text file's table of section offsets into
 * g_text_section_pointers. The offsets are relative to the end of the
 * table. */
void text_init_section_pointers_variant(void) {
    s32* offset;
    u8** section;
    s32 i;
    u8* text;
    s32 value;

    offset = g_text_section_offsets;
    i = 0;
    text = (u8*)(offset + TEXT_SECTION_COUNT);
    section = g_text_section_pointers;
    for (; i < TEXT_SECTION_COUNT; i++) {
        value = *offset;
        offset++;
        value += (s32)text;
        *section = (u8*)value;
        section++;
    }
}
