#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0x90 "End Bar & Loop": jumps back to the loop point set
 * by Loop (0x91), counting the pass and restoring the octave. Without a loop
 * point the channel ends: its SPU voice is queued for key-off (the SFX MUS
 * record also drops the voice from its LFO, noise and reverb masks and from
 * the SFX reservation, and marks envelope and LFO parameters for update),
 * and the read position stays on this opcode. */
u8* main_smd_end_bar(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 mask;

    if (channel->loop_note_data) {
        note_data = channel->loop_note_data;
        channel->loop_count++;
        channel->octave_base = channel->loop_octave_base;
    } else {
        channel->note_flags2 &= ~3;
        if (music->status & 2) {
            g_main_sound_sfx_key_off_voices |= channel->voice_mask;
            mask = ~channel->voice_mask;
            music->lfo_voice_mask &= mask;
            music->noise_voice_mask &= mask;
            music->reverb_voice_mask &= mask;
            g_main_mask_exclusion &= mask;
            channel->func_flags |= 0x54;
        } else {
            g_main_sound_music_key_off_voices |= channel->voice_mask;
        }
        channel->active = 0;
        note_data--;
    }
    return note_data;
}
