#include "fft/suzuki.h"
#include "psx/types.h"

/* The transfer controller indexes these records in 16-byte steps. */
typedef struct suzuki_spu_transfer_status {
    s16 status; /* 0x00 */
    u8 _unused_02[14];
} suzuki_spu_transfer_status_t;

extern u16 g_suzuki_sound_spu_transfer_status_index; /* index into g_suzuki_sound_spu_transfer_status_records */
extern suzuki_spu_transfer_status_t* g_suzuki_sound_spu_transfer_status_records;

/* Volatile view: the target re-reads the transfer flag on every poll. */
extern volatile s16 g_suzuki_sound_driver_flags;

/* With SUZUKI_SOUND_SPU_TRANSFER_WAIT in flags, spin until the SPU transfer
 * callback clears the transfer-busy flag. Returns the first 16-bit field of the
 * current transfer status record while a transfer is still running, else 0. */
s16 suzuki_sound_wait_for_spu_transfer(s32 flags) {
    if (flags & SUZUKI_SOUND_SPU_TRANSFER_WAIT) {
        while (g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SPU_TRANSFER) { }
    }
    if (g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SPU_TRANSFER) {
        return g_suzuki_sound_spu_transfer_status_records[g_suzuki_sound_spu_transfer_status_index].status;
    }
    return 0;
}
