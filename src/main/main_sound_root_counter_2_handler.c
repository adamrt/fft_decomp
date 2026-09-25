#include "fft/main.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Event handler of root counter 2 (g_main_root_counter_2_event): commits
 * pending voice attributes and key-ons, steps the driver volume ramps on
 * every second call, then advances each playing MUS record by as many
 * sequencer ticks as its tempo accumulator (tempo_accumulator, less tempo * scale per
 * call, plus 0x10000 per tick) allows, and finally commits the key-offs.
 *
 * bar/beat count bars and beats (beat_ticks_36 ticks per beat); the record
 * is stopped when its master volume fades to zero or its bar count reaches
 * stop_bar, and stops playing once no channel is running.
 *
 * Both key flushes are inlined from main_sound_key_flush.h; the generated
 * linker script only binds names that appear in this file, so the ones they
 * add are listed here: g_main_mask_exclusion, g_main_sound_music_key_off_voices, g_main_sound_sfx_key_off_voices,
 * SpuSetVoiceRRAttr and SpuSetKey. */
s32 main_sound_root_counter_2_handler(void) {
    suzuki_music_t* music;
    u16 requests;
    s16 count; /* the per-channel passes take an s16 count */

    main_sound_update_voice_attrs();
    requests = g_main_sound_voice_update_requests;
    if ((requests & 1) != 0)
        main_sound_calculate_pitch_lfo_voices();
    if ((requests & 2) != 0)
        main_sound_calculate_noise_voices();
    if ((requests & 4) != 0)
        main_sound_calculate_reverb_voices();
    g_main_sound_voice_update_requests = 0;
    main_sound_flush_key_on_inline();

    if ((g_main_sound_tick_count++ & 1) != 0) {
        if (g_main_sound_spu_state.music_ramp.count != 0) {
            main_smd_step_ramp(&g_main_sound_spu_state.music_ramp);
            g_main_sound_spu_state.music_volume = g_main_sound_spu_state.music_ramp.value >> 16;
            main_sound_set_vol_balance(g_main_sound_spu_state.music_volume, &g_main_sound_spu_state.common.mvol, 0);
            g_main_sound_spu_state.common.mask |= 3;
        }
        if (g_main_sound_spu_state.cd_ramp.count != 0) {
            main_smd_step_ramp(&g_main_sound_spu_state.cd_ramp);
            g_main_sound_spu_state.cd_volume = g_main_sound_spu_state.cd_ramp.value >> 16;
            main_sound_set_vol_balance(g_main_sound_spu_state.cd_volume, &g_main_sound_spu_state.common.cd.volume, 0);
            g_main_sound_spu_state.common.mask |= 0xc0;
        }
        if (g_main_sound_spu_state.common.mask != 0) {
            SpuSetCommonAttr(&g_main_sound_spu_state.common);
            g_main_sound_spu_state.common.mask = 0;
        }
    }

    music = g_main_sound_active_music_list;
    while (music != 0) {
        /* The playing bit is tested as a sign (`lh`, `bgez`). */
        if ((s16)music->status < 0) {
            if (music->tick_2c != 0 && music->tick_24 >= music->tick_2c)
                main_smd_restore_snapshot(music);
            if (music->tempo_scale.count != 0) {
                main_smd_step_ramp(&music->tempo_scale);
                music->scaled_tempo = music->tempo.halves.high * (music->tempo_scale.value >> 16);
            }
            if (music->master_volume.count != 0) {
                main_smd_step_ramp(&music->master_volume);
                main_smd_set_note_flags2_all_channels(0x100, music);
            }
            if (music->pitch_shift.count != 0) {
                main_smd_step_ramp(&music->pitch_shift);
                main_smd_set_note_flags2_all_channels(0x200, music);
            }
            if (music->balance_shift.count != 0) {
                main_smd_step_ramp(&music->balance_shift);
                main_smd_set_note_flags2_all_channels(0x100, music);
            }
            music->tick_20++;
            music->ticks += music->tempo_scale.value >> 16;
            music->tempo_accumulator -= music->scaled_tempo;
            while (music->tempo_accumulator < 0) {
                music->tempo_accumulator += 0x10000;
                if (--music->beat_ticks_36 == 0) {
                    music->beat_ticks_36 = music->ticks_per_beat;
                    if (++music->beat > music->beats_per_bar) {
                        music->beat = 1;
                        music->bar++;
                    }
                }
                count = music->channel_count;
                if (count != 0) {
                    main_smd_update_channel_ramps(music, music->channels, count);
                    main_smd_read_instructions(music, music->channels, count);
                    main_smd_update_modulators(music, music->channels, count);
                    main_smd_update_voices(music, music->channels, count);
                }
                if (music->channel_mask != 0) {
                    music->tick_24++;
                    if (music->master_volume.value == 0) {
                        SuzukiDeallocateMUSChannels(music);
                        music->status |= 0x4100;
                    }
                    if (music->bar == music->stop_bar)
                        SuzukiDeallocateMUSChannels(music);
                } else {
                    music->status &= 0x7fff;
                    break;
                }
            }
        }
        music = music->next;
    }

    main_sound_flush_key_off_inline();
    return 0;
}
