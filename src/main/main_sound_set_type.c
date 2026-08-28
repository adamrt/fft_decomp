#include "fft/main_sound.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Selects the output sound type and reapplies the volumes, reverb and CD mix
 * that depend on it.
 *
 * Type 0 (mono) and any value outside 1-3 leave the status type bits clear;
 * 1 (stereo) sets 0x100, 2 (wide) 0x300 and 3 0x500 (no menu offers it).
 * The explicit case 0 makes the target's compare tree (root at case 1). */
void main_sound_set_type(s32 type) {
    suzuki_music_t* music;

    g_main_sound_driver_flags &= 0xf8ff;
    switch (type) {
    case 0:
        break;
    case 1:
        g_main_sound_driver_flags |= 0x100;
        break;
    case 2:
        g_main_sound_driver_flags |= 0x300;
        break;
    case 3:
        g_main_sound_driver_flags |= 0x500;
        break;
    }
    main_sound_commit_volume_change();
    SpuSetReverbModeParam(&g_main_sound_reverb_attr);
    g_main_sound_reverb_attr.mask = 0;
    for (music = g_main_sound_active_music_list; music != 0; music = music->next) {
        main_smd_set_note_flags2_all_channels(0x100, music);
    }
    if (g_main_sound_driver_flags & 0x2000) {
        main_sound_put_type(g_main_sound_spu_state.sound_type);
    }
}
