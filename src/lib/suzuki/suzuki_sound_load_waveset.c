#include "fft/suzuki.h"
#include "psx/types.h"

/* Uploads a loaded WAVESET.WD to SPU RAM and appends a heap copy of its
 * header to g_suzuki_sound_waveset_list.
 *
 * The first list test reads the head global itself, not through link. */
suzuki_waveset_t* suzuki_sound_load_waveset(suzuki_waveset_t* file) {
    u32 spu_address = suzuki_sound_alloc_spu_ram(file->wave_size);
    suzuki_waveset_t* waveset;
    suzuki_waveset_t** link;

    suzuki_sound_transfer_spu_data(spu_address, (u8*)file + file->wave_offset, file->wave_size,
        SUZUKI_SOUND_SPU_TRANSFER_WAIT | SUZUKI_SOUND_SPU_TRANSFER_WRITE);
    waveset = suzuki_sound_alloc(file->header_size);
    suzuki_sound_copy_memory(waveset, file, file->header_size);
    waveset->spu_address = spu_address;
    link = &g_suzuki_sound_waveset_list;
    if (g_suzuki_sound_waveset_list != 0) {
        do {
            link = &(*link)->next;
        } while (*link != 0);
    }
    *link = waveset;
    waveset->next = 0;
    suzuki_sound_wait_for_spu_transfer(SUZUKI_SOUND_SPU_TRANSFER_WAIT);
    return waveset;
}
