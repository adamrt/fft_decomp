#include "fft/battle.h"
#include "fft/battle_text.h"
#include "psx/types.h"

/* Relocate the 31 embedded menu-text offsets into the active section table.
 *
 * The text begins after a 32-word offset header. When menu input is disabled,
 * section 0 remains bound to the event/tutorial text already in use. */
void battle_text_init_menu_section_pointers(void) {
    s32* source;
    u8** destination;
    /* These two assignments fill the target load delay before the loop. */
    s32* text;
    s32 menu_input_disabled;
    s32 index;
    s32 offset;

    source = g_battle_menu_text_section_offsets;
    index = 0;
    menu_input_disabled = g_battle_menu_input_disabled;
    text = source + 32;
    destination = g_battle_text_section_pointers;
    do {
        if (menu_input_disabled == 0 || index != 0) {
            offset = *source;
            offset += (s32)text;
            *destination = (u8*)offset;
        }
        source++;
        index++;
        destination++;
    } while (index < 31);
}
