#include "fft/battle_text.h"
#include "psx/types.h"

void attack_text_init_battle_pointers(s32* offsets) {
    s32 i;
    u8* data;
    u8** table;

    i = 0;
    /* Section data follows the 32-entry offset header. */
    data = (u8*)&offsets[32];
    table = g_battle_text_section_pointers;
    do {
        if (i != 0) {
            *table = data + *offsets;
        }
        offsets++;
        i++;
        table++;
    } while (i < 32);
}
