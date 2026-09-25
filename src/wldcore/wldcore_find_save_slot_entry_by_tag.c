#include "fft/wldcore.h"
#include "psx/types.h"

/* Scan the selected save slot's word list for an entry whose low byte matches
 * `tag`; a low byte of 1 terminates the list. Returns the whole word or 0. */
u32 wldcore_find_save_slot_entry_by_tag(s32 slot, s32 tag) {
    u32* word;
    s32 i;

    word = &((u32*)g_main_save_slot_buffer)[g_wldcore_script_slot_table->offsets[slot] >> 2];
    for (i = 0; i < 256; i++) {
        u32 value = *word;
        if (tag == (value & 0xFF)) {
            return value;
        }
        if ((value & 0xFF) == 1) {
            return 0;
        }
        word++;
    }
    return 0;
}
