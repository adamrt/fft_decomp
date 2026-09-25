#include "fft/main.h"
#include "psx/types.h"

/* Uploads a loaded WAVESET.WD to SPU RAM and appends a heap copy of its
 * header to g_main_sound_waveset_list.
 *
 * The first list test reads the head global itself, not through link. */
suzuki_waveset_t* main_sound_load_waveset(suzuki_waveset_t* file) {
    u32 spu_address;
    suzuki_waveset_t* waveset;
    suzuki_waveset_t** link;

    spu_address = main_sound_alloc_spu_ram(file->wave_size);
    main_sound_transfer_spu_data(spu_address, (u8*)file + file->wave_offset, file->wave_size, 0x11);
    waveset = main_sound_alloc(file->header_size);
    main_sound_copy_memory(waveset, file, file->header_size);
    waveset->spu_address = spu_address;
    link = &g_main_sound_waveset_list;
    if (g_main_sound_waveset_list != 0) {
        do {
            link = &(*link)->next;
        } while (*link != 0);
    }
    *link = waveset;
    waveset->next = 0;
    main_sound_wait_for_spu_transfer(0x10);
    return waveset;
}
