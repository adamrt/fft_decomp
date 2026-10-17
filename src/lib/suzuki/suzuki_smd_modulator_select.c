#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xF0 "Modulator Select": selects the modulator that 0xF1 and
 * 0xF2 program and sets its waveform (low nibble of the second byte; it
 * restarts on key-on unless bit 4 is set) and output target (third byte,
 * suzuki_modulator_target_e). The modulator is left inactive. */
u8* suzuki_smd_modulator_select(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator;
    u8 param;

    channel->modulator_index = note_data[0];
    modulator = &channel->modulators[channel->modulator_index];
    param = note_data[1];
    modulator->waveform = param & SUZUKI_MODULATOR_INPUT_WAVEFORM_MASK;
    modulator->step = g_suzuki_smd_modulator_waveforms[modulator->waveform];
    if ((param & SUZUKI_MODULATOR_INPUT_NO_KEY_ON_RESTART) == 0) {
        modulator->flags = SUZUKI_MODULATOR_RESTART_ON_KEY_ON;
    } else {
        modulator->flags = 0;
    }
    modulator->target = note_data[2];
    modulator->delay_reset = 0;
    modulator->fade_step = 0x100;
    return note_data + 3;
}
