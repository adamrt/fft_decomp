/* SCUS_942.21 0x80018858..0x80018aeb. */
#include "psx/libspu.h"

#include "psx/libc.h"

/* Reset the SPU: enable its DMA channel, silence and stop it, key every voice off and clear reverb
 * and the key queue. A cold start (hot 0) also zeroes each voice and the noise, LFO, CD and
 * external inputs, uploads a 16-byte block at SPU address 0x200 that every voice is pointed at,
 * and keys all voices on then off. Ends with the SPU enabled (control 0xc000). */
s32 _spu_init(s32 hot) {
    u32 wait;
    s32 i;
    *g_psyq_spu_dma_priority_register |= 0xb0000;
    _spu_transMode = SPU_TRANSFER_BY_DMA;
    _spu_addrMode = 0;
    _spu_tsa = 0;
    _spu_RXX->rxx.main_volume_left = 0;
    _spu_RXX->rxx.main_volume_right = 0;
    _spu_RXX->rxx.control = 0;
    _spu_Fw1ts();
    _spu_RXX->rxx.main_volume_left = 0;
    _spu_RXX->rxx.main_volume_right = 0;
    wait = 0;
    while (_spu_RXX->rxx.status & PSYQ_SPU_STATUS_MASK) {
        if (++wait > PSYQ_SPU_POLL_LIMIT) {
            printf(g_psyq_spu_timeout_format, g_psyq_spu_init_timeout_name);
            break;
        }
    }
    _spu_mem_mode = 2;
    _spu_mem_mode_plus = 3;
    _spu_mem_mode_unit = 8;
    _spu_mem_mode_unitM = 7;
    _spu_RXX->rxx.transfer_control = 4;
    _spu_RXX->rxx.reverb_depth_left = 0;
    _spu_RXX->rxx.reverb_depth_right = 0;
    _spu_RXX->rxx.key_off_low = 0xffff;
    _spu_RXX->rxx.key_off_high = 0xffff;
    _spu_RXX->rxx.reverb_low = 0;
    _spu_RXX->rxx.reverb_high = 0;
    for (i = 0; i < 10; i++) {
        _spu_RQ[i] = 0;
    }
    if (!hot) {
        _spu_tsa = 0x200;
        _spu_RXX->rxx.pitch_lfo_low = 0;
        _spu_RXX->rxx.pitch_lfo_high = 0;
        _spu_RXX->rxx.noise_low = 0;
        _spu_RXX->rxx.noise_high = 0;
        _spu_RXX->rxx.cd_volume_left = 0;
        _spu_RXX->rxx.cd_volume_right = 0;
        _spu_RXX->rxx.external_volume_left = 0;
        _spu_RXX->rxx.external_volume_right = 0;
        _spu_writeByIO(g_psyq_spu_init_fifo_data, 16);
        for (i = 0; i < 24; i++) {
            _spu_RXX->rxx.voices[i].volume_left = 0;
            _spu_RXX->rxx.voices[i].volume_right = 0;
            _spu_RXX->rxx.voices[i].pitch = 0x3fff;
            _spu_RXX->rxx.voices[i].start_address = 0x200;
            _spu_RXX->rxx.voices[i].adsr1 = 0;
            _spu_RXX->rxx.voices[i].adsr2 = 0;
        }
        _spu_RXX->rxx.key_on_low = 0xffff;
        _spu_RXX->rxx.key_on_high = 0xff;
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_RXX->rxx.key_off_low = 0xffff;
        _spu_RXX->rxx.key_off_high = 0xff;
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
    }
    _spu_inTransfer = 1;
    _spu_RXX->rxx.control = 0xc000;
    _spu_transferCallback = 0;
    _spu_IRQCallback = 0;
    return 0;
}
