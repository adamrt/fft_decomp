#include "fft/suzuki.h"

/* Resets the playback state of a MUS record: frees its snapshots, clears the
 * counters, voice masks and ramps, and sets 4/4 time, tempo 0x66 and full
 * master volume. */
void suzuki_smd_init_music_header(suzuki_music_t* music) {
    suzuki_smd_free_snapshots(music);
    music->_unknown_01c = 0;
    music->loop_select = 0;
    music->min_loop_count = 0;
    music->bar = 1;
    music->beat = 0;
    music->beat_ticks_remaining = 1;
    music->ticks = 0;
    music->sequence_ticks = 0;
    music->timer_ticks = 0;
    music->channel_mask = 0;
    music->key_on_mask = 0;
    music->key_off_mask = 0;
    music->lfo_voice_mask = 0;
    music->noise_voice_mask = 0;
    music->reverb_voice_mask = 0;
    music->pitch_shift.value = 0;
    music->balance_shift.value = 0;
    music->tempo_scale.count = 0;
    music->master_volume.count = 0;
    music->pitch_shift.count = 0;
    music->balance_shift.count = 0;
    music->tempo_step = 0;
    music->tempo_step_count = 0;
    music->beats_per_bar = 4;
    music->beat_unit = 4;
    music->beats_per_bar_copy = 4;
    music->tempo_scale.value = 0x1000000;
    music->master_volume.value = 0x7f000000;
    music->tempo = 0x660000;
    music->scaled_tempo = 0x6600;
    music->tempo_accumulator = 0x10000;
    music->ticks_per_beat = 0x30 / music->tick_divisor;
}
