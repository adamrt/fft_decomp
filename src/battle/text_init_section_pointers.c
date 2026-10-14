#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Relocate the 31 embedded menu-text offsets into the active section table.
 *
 * The text begins after a header of 32 32-bit offsets. When menu input is disabled,
 * section 0 remains bound to the event/tutorial text already in use. */
void text_init_section_pointers(void) {
    s32* source;
    u8** destination;
    /* These two assignments fill the target load delay before the loop. */
    s32* text;
    s32 menu_input_mode;
    s32 index;
    s32 offset;

    source = g_battle_menu_text_section_offsets;
    index = 0;
    menu_input_mode = g_menu_input_mode;
    text = source + 32;
    destination = g_text_section_pointers;
    do {
        if (menu_input_mode == MENU_INPUT_PLAYER || index != TEXT_SECTION_EVENT_MESSAGES) {
            offset = *source;
            offset += (s32)text;
            *destination = (u8*)offset;
        }
        source++;
        index++;
        destination++;
    } while (index < 31);
}
