#include "fft/world.h"
#include "psx/types.h"

void world_text_init_section_pointers(void) {
    s32* offset;
    u8** section;
    s32 i;
    u8* text;
    s32 value;

    offset = g_world_text_section_offsets;
    i = 0;
    text = (u8*)(offset + WORLD_TEXT_SECTION_COUNT);
    section = g_world_text_section_pointers;
    do {
        value = *offset;
        offset++;
        i++;
        value += (s32)text;
        *section = (u8*)value;
        section++;
    } while (i < WORLD_TEXT_SECTION_COUNT);
}
