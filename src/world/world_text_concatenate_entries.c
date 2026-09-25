#include "fft/world.h"
#include "psx/types.h"

enum {
    WORLD_TEXT_ENTRY_END = 0xFE,
    WORLD_TEXT_ENTRY_SEPARATOR = 0xF8,
};

/* Concatenate selected entries into one terminated WORLD text stream.
 *
 * Separators are optional; when requested, the final separator is replaced by
 * the stream terminator instead of being emitted after the last entry. */
void world_text_concatenate_entries(u8* text_table, u8* destination, const s16* text_ids, s32 separate_entries) {
    u8* output;
    const s16* current_id;
    s32 separator;
    u8* source;
    s32 call_index;
    s32 compare_v0;
    /* Pin required: unpinned, source and the 0xfe terminator swap $a0 and $v1. */
    register s32 compare_v1 __asm__("$3");
    u8 value;

    output = destination;
    separator = separate_entries;
    if (*text_ids != -1) {
        current_id = text_ids;
        do {
            call_index = *(u16*)current_id;
            source = ((u8 * (*)(u8*, s32, s32)) world_text_find_entry_by_index)(
                text_table, call_index & TEXT_ID_ENTRY_MASK, 1);
            compare_v1 = *source;
            if (compare_v1 != WORLD_TEXT_ENTRY_END) {
                compare_v1 = WORLD_TEXT_ENTRY_END;
                do {
                    value = *source;
                    source++;
                    *output = value;
                    output++;
                    compare_v0 = *source;
                } while (compare_v0 != compare_v1);
            }
            current_id++;
            if (separator != 0) {
                *output = WORLD_TEXT_ENTRY_SEPARATOR;
                output++;
            }
        } while (*current_id != -1);
    }
    if (separator != 0) {
        output--;
    }
    *output = WORLD_TEXT_ENTRY_END;
}
