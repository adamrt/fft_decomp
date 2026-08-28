#include "psx/types.h"

/* Volatile view: the target re-reads the transfer flag on every poll. */
extern volatile u16 g_main_sound_driver_flags;
extern s16* g_main_sound_spu_transfer_status_records;
extern u16 g_main_sound_spu_transfer_status_index;

s16 main_sound_wait_for_spu_transfer(s32 flags) {
    if (flags & 0x10) {
        while (g_main_sound_driver_flags & 0x10) { }
    }
    if (g_main_sound_driver_flags & 0x10) {
        return g_main_sound_spu_transfer_status_records[g_main_sound_spu_transfer_status_index * 8];
    }
    return 0;
}
