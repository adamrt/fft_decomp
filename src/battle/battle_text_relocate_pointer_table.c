#include "fft/battle_text.h"
#include "psx/types.h"

void battle_text_relocate_pointer_table(const u32* offsets) {
    s32 index;
    const u8* text_data;

    index = 0;
    text_data = (const u8*)offsets + sizeof(u32) * 32;
    do {
        g_battle_text_section_pointers[index] = (u8*)(text_data + offsets[index]);
        index++;
    } while (index < 32);
}
