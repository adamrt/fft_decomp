#include "fft/event_card.h"
#include "psx/types.h"

s32 card_file_count_free_blocks(const card_directory_entry_t* entries, s32 entry_count) {
    s32 i;
    s32 used_blocks;

    used_blocks = 0;
    for (i = 0; i < entry_count; i++) {
        used_blocks += entries[i].size >> 13;
        used_blocks += !!(entries[i].size % 0x2000);
    }

    if (used_blocks >= 16) {
        return 0;
    }
    return 15 - used_blocks;
}
