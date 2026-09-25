#include "fft/world.h"

u8* world_text_find_entry(s32 combined_text_index) {
    return (u8*)world_text_skip_to_entry(
        g_world_text_section_pointers[(combined_text_index & TEXT_ID_SECTION_MASK) >> TEXT_ID_SECTION_SHIFT],
        combined_text_index & TEXT_ID_ENTRY_MASK);
}
