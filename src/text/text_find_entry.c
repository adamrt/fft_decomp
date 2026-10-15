#include "fft/menu.h"

/* Returns the start of a text entry. The text id holds the section in its high
 * part and the entry number in its low part (TEXT_ID_SECTION_MASK and
 * TEXT_ID_ENTRY_MASK). */
u8* text_find_entry(u32 text_id) {
    return (u8*)text_skip_to_entry(g_text_section_pointers[(text_id & TEXT_ID_SECTION_MASK) >> TEXT_ID_SECTION_SHIFT],
        text_id & TEXT_ID_ENTRY_MASK);
}
