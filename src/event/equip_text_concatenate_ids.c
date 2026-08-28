#include "fft/equip.h"
#include "psx/types.h"

/* Concatenate the text for each entry of a -1 terminated id list into dst,
   optionally separating entries with 0xF8, and terminate with 0xFE. */
void equip_text_concatenate_ids(s32 text_table, u8* dst, s16* list, s32 separate) {
    u8* src;
    s16* entry;

    if (*list != -1) {
        entry = list;
        do {
            src = (u8*)((const u8* (*)(s32, s32, s32))equip_text_skip_encoded_segments)(
                text_table, *(u16*)entry & 0x7FF, 1);
            while (*src != 0xFE) {
                *dst = *src;
                src++;
                dst++;
            }
            entry++;
            if (separate != 0) {
                *dst = 0xF8;
                dst++;
            }
        } while (*entry != -1);
    }

    if (separate != 0) {
        dst--;
    }
    *dst = 0xFE;
}
