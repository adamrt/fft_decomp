#include "fft/world.h"
#include "psx/types.h"

/* Converts the text file's section offset table into absolute section pointers. */
void world_text_init_format_section_pointers(s32* offsets) {
    s32 i = 0;
    s32 base = (s32)offsets + WORLD_TEXT_FILE_HEADER_BYTES;
    s32* dst = (s32*)g_world_text_section_pointers;
    do {
        *dst++ = base + *offsets++;
        i++;
    } while (i < WORLD_TEXT_SECTION_COUNT);
}
