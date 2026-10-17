#include "fft/suzuki.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* Event handler of root counter 2 (g_suzuki_sound_root_counter_2_event): commits
 * pending voice attributes and key-ons, steps the driver volume ramps on
 * every second call, then advances each playing MUS record by as many
 * sequencer ticks as its tempo accumulator (tempo_accumulator, less tempo * scale per
 * call, plus 0x10000 per tick) allows, and finally commits the key-offs.
 *
 * bar/beat count bars and beats (beat_ticks_remaining ticks per beat); the record
 * is stopped when its master volume fades to zero or its bar count reaches
 * stop_bar, and stops playing once no channel is running.
 */
s32 suzuki_sound_root_counter_2_handler(void) {
    suzuki_music_t* music;
    u16 requests;
    s16 count; /* the per-channel passes take an s16 count */

    suzuki_sound_update_voice_attrs();
    requests = g_suzuki_sound_voice_update_requests;
    if ((requests & SUZUKI_VOICE_MASK_REQUEST_PITCH_LFO) != 0)
        suzuki_sound_calculate_pitch_lfo_voices();
    if ((requests & SUZUKI_VOICE_MASK_REQUEST_NOISE) != 0)
        suzuki_sound_calculate_noise_voices();
    if ((requests & SUZUKI_VOICE_MASK_REQUEST_REVERB) != 0)
        suzuki_sound_calculate_reverb_voices();
    g_suzuki_sound_voice_update_requests = 0;
    suzuki_sound_flush_key_on_inline();

    if ((g_suzuki_sound_tick_count++ & 1) != 0) {
        if (g_suzuki_sound_spu_state.music_ramp.count != 0) {
            suzuki_smd_step_ramp(&g_suzuki_sound_spu_state.music_ramp);
            g_suzuki_sound_spu_state.music_volume = g_suzuki_sound_spu_state.music_ramp.value >> 16;
            suzuki_sound_set_vol_balance(
                g_suzuki_sound_spu_state.music_volume, &g_suzuki_sound_spu_state.common.mvol, 0);
            g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_MVOLL | SPU_COMMON_MVOLR;
        }
        if (g_suzuki_sound_spu_state.cd_ramp.count != 0) {
            suzuki_smd_step_ramp(&g_suzuki_sound_spu_state.cd_ramp);
            g_suzuki_sound_spu_state.cd_volume = g_suzuki_sound_spu_state.cd_ramp.value >> 16;
            suzuki_sound_set_vol_balance(
                g_suzuki_sound_spu_state.cd_volume, &g_suzuki_sound_spu_state.common.cd.volume, 0);
            g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
        }
        if (g_suzuki_sound_spu_state.common.mask != 0) {
            SpuSetCommonAttr(&g_suzuki_sound_spu_state.common);
            g_suzuki_sound_spu_state.common.mask = 0;
        }
    }

    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        /* The playing bit (15) is the sign. */
        if (music->status < 0) {
            if (music->snapshot_restore_tick != 0 && music->sequence_ticks >= music->snapshot_restore_tick)
                suzuki_smd_restore_snapshot(music);
            if (music->tempo_scale.count != 0) {
                suzuki_smd_step_ramp(&music->tempo_scale);
                music->scaled_tempo = (music->tempo >> 16) * (music->tempo_scale.value >> 16);
            }
            if (music->master_volume.count != 0) {
                suzuki_smd_step_ramp(&music->master_volume);
                suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_VOLUME, music);
            }
            if (music->pitch_shift.count != 0) {
                suzuki_smd_step_ramp(&music->pitch_shift);
                suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_PITCH, music);
            }
            if (music->balance_shift.count != 0) {
                suzuki_smd_step_ramp(&music->balance_shift);
                suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_VOLUME, music);
            }
            music->timer_ticks++;
            music->ticks += music->tempo_scale.value >> 16;
            music->tempo_accumulator -= music->scaled_tempo;
            while (music->tempo_accumulator < 0) {
                music->tempo_accumulator += 0x10000;
                if (--music->beat_ticks_remaining == 0) {
                    music->beat_ticks_remaining = music->ticks_per_beat;
                    if (++music->beat > music->beats_per_bar) {
                        music->beat = 1;
                        music->bar++;
                    }
                }
                count = music->channel_count;
                if (count != 0) {
                    suzuki_smd_update_channel_ramps(music, music->channels, count);
                    suzuki_smd_read_instructions(music, music->channels, count);
                    suzuki_smd_update_modulators(music, music->channels, count);
                    suzuki_smd_update_voices(music, music->channels, count);
                }
                if (music->channel_mask != 0) {
                    music->sequence_ticks++;
                    if (music->master_volume.value == 0) {
                        suzuki_smd_stop_music(music);
                        music->status |= (SUZUKI_MUSIC_STATUS_PAUSED | SUZUKI_MUSIC_STATUS_FULL_VOICE_UPDATE);
                    }
                    if (music->bar == music->stop_bar)
                        suzuki_smd_stop_music(music);
                } else {
                    music->status &= ~SUZUKI_MUSIC_STATUS_PLAYING;
                    break;
                }
            }
        }
        music = music->next;
    }

    suzuki_sound_flush_key_off_inline();
    return 0;
}
