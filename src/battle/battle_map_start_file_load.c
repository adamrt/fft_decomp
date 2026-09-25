#include "psx/types.h"

/* `load_info` points to +0x06 of a raw GNS file row. Relative offsets
 * 0x02/0x06 hold its 32-bit absolute disc LBA and byte length; +0x0d is the
 * nonzero file marker at row offset 0x13. */
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
