#include "fft/main_sound.h"
#include "psx/api.h"
#include "psx/etc.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Starts the Suzuki driver once (status bit 15 marks it initialised).
 *
 * flags seeds the status word. Sets up SPU RAM allocation and the driver
 * heap, clears the driver lists, creates the SFX music record, installs the
 * root-counter-2 tick and SPU events and the transfer callback, then applies
 * sound type 1, reverb mode 4, CD mixing and the default volumes. */
void SuzukiSPUInitialiser(s32 flags) {
    /* Bit 15 (initialised) is tested as a sign, on an `lh` load. */
    if ((s16)g_main_sound_driver_flags < 0) {
        return;
    }
    g_main_sound_driver_flags = flags | 0x8000;
    SpuInitMalloc(6, g_main_sound_spu_malloc_table);
    main_sound_init_heap(g_main_sound_heap_memory, 0x8000);
    main_noop_80014544();
    g_main_smd_random_state = 0x12345678;
    g_main_sound_active_music_list = 0;
    g_main_sound_sfx_music = 0;
    g_main_sound_resource_list = 0;
    g_main_sound_waveset_list = 0;
    g_main_mask_exclusion = 0;
    g_main_sound_sfx_restart_channels = 0;
    g_main_sound_sfx_restart_voices = 0;
    g_main_sound_music_key_off_voices = 0;
    g_main_sound_sfx_key_off_voices = 0;
    g_main_sound_sfx_instrument = 0;
    D_80032A58 = 0;
    g_main_sound_sfx_music = main_sound_init_sfx_music();
    g_main_sound_spu_state.common.mvolmode.left = 0;
    g_main_sound_spu_state.common.mvolmode.right = 0;
    g_main_sound_spu_state.common.mask = 0xc;
    EnterCriticalSection();
    g_main_root_counter_2_event = OpenEvent(0xf2000002, 2, 0x1000, (s32)main_sound_root_counter_2_handler);
    g_main_sound_spu_event = OpenEvent(0xf0000009, 0x1000, 0x1000, (s32)main_sound_spu_event_handler);
    g_main_sound_tick_count = 0;
    D_80032A34 = 0;
    SetRCnt(0xf2000002, 0x44e8, 0x1000);
    StartRCnt(0xf2000002);
    EnableEvent(g_main_root_counter_2_event);
    SpuSetTransferCallback(main_suzuki_spu_callback_func);
    g_main_sound_driver_flags |= 1;
    ExitCriticalSection();
    main_sound_set_type(1);
    g_main_sound_reverb_attr.mode = -1;
    main_sound_set_reverb_mode(4, 0, 0, 0);
    main_sound_set_cd_reverb(0, 1);
    main_sound_set_master_volume(0x3fff, 0);
    main_sound_set_cd_volume(0x6400, 0);
    if (g_main_sound_driver_flags & 0x2000) {
        main_sound_put_type(0xc0);
    }
    main_sound_set_sfx_enabled(1);
    D_80032A28 = 0;
}
