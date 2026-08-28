#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xF0: selects the modulator that 0xF1 and
 * 0xF2 program and sets its waveform (low nibble of the second byte; bit 4
 * clear sets flags bit 1) and output target (third byte). */
u8* main_smd_modulator_select(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator;
    u8 param;

    channel->modulator_index = note_data[0];
    modulator = &channel->modulators[channel->modulator_index];
    param = note_data[1];
    modulator->waveform = param & 0xf;
    modulator->step = g_main_smd_modulator_waveforms[modulator->waveform];
    if ((param & 0x10) == 0) {
        modulator->flags = 2;
    } else {
        modulator->flags = 0;
    }
    modulator->target = note_data[2];
    modulator->counter_14_reset = 0;
    modulator->counter_18_reset = 0x100;
    return note_data + 3;
}
