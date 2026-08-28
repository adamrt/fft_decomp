#include "fft/main_sound.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Starts an SPU RAM transfer at spu_address once the previous one is done.
 *
 * The low nibble of mode selects it: 1 writes size bytes from data, 2 reads
 * them into data, 3 and 4 read the decoded CD/voice buffers (all, or CD only)
 * with the result in g_main_sound_decoded_data_result. The status bits 0x10-0x40 stay set until
 * the transfer callback clears them. The empty case 5 is part of the
 * target's five-entry jump table. */
void main_sound_transfer_spu_data(u32 spu_address, void* data, u32 size, s32 mode) {
    main_sound_wait_for_spu_transfer(0x10);
    g_main_sound_driver_flags |= 0x10;
    SpuSetTransferMode(0);
    SpuSetTransferStartAddr(spu_address);
    switch (mode & 0xf) {
    case 1:
        g_main_sound_driver_flags |= 0x30;
        SpuWrite(data, size);
        break;
    case 2:
        g_main_sound_driver_flags |= 0x50;
        SpuRead(data, size);
        break;
    case 3:
        g_main_sound_driver_flags |= 0x50;
        g_main_sound_decoded_data_result = SpuReadDecodedData(data, 0);
        break;
    case 4:
        g_main_sound_driver_flags |= 0x50;
        g_main_sound_decoded_data_result = SpuReadDecodedData(data, 5);
        break;
    case 5:
        break;
    }
}
