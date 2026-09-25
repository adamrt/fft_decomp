#include "fft/world.h"
#include "psx/types.h"

/* Converts the text file's section offset table into absolute section pointers. */
void world_text_init_format_section_pointers(s32* offsets) {
    s32 i = 0;
    u8* base = (u8*)offsets + WORLD_TEXT_FILE_HEADER_BYTES;
    u8** dst = g_world_text_section_pointers;
    do {
        *dst++ = base + *offsets++;
        i++;
    } while (i < WORLD_TEXT_SECTION_COUNT);
}
