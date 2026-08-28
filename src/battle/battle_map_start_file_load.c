#include "psx/types.h"

/* Target 0x800f36c4 consumes processed load information, not a raw
 * gns_file_record_t. Relative offsets 0x02/0x06 hold full 32-bit LBA/length
 * values and 0x0d is presence; raw GNS instead has a 16-bit relative sector
 * at 0x08. The caller supplies this view from its separate descriptor table. */
enum {
    MAP_FILE_LOAD_LBA_OFFSET = 0x02,
    MAP_FILE_LOAD_BYTE_LENGTH_OFFSET = 0x06,
    MAP_FILE_LOAD_PRESENT_OFFSET = 0x0d,
};

s32 battle_map_start_file_load(void* unused_file_table, u8* load_info, s32 destination, s32 suppress_loading_display) {
    if (load_info[MAP_FILE_LOAD_PRESENT_OFFSET] != 0) {
        /* Makes GCC re-copy destination from $s0 into $a2 for the call; without
         * the clobber it knows $a2 still holds it and leaves a nop in the slot. */
        asm volatile("" : : : "$6");
        if (main_file_call_build_header(*(s32*)(load_info + MAP_FILE_LOAD_LBA_OFFSET),
                *(s32*)(load_info + MAP_FILE_LOAD_BYTE_LENGTH_OFFSET), (void*)destination)
            == 0) {
            return destination;
        }
    }
    return 0;
}
