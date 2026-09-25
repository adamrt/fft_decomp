#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "psx/types.h"

/* Disc sectors and byte budgets of the resident sound files (LBAs from the
 * disc inventory: WAVESET.WD 85007, SYSTEM.SED 85002, ENV.SED 85000). */
#define WAVESET_WD_SECTOR 0x14C0F
#define WAVESET_WD_SIZE   0x79000
#define SYSTEM_SED_SECTOR 0x14C0A
#define SYSTEM_SED_SIZE   0x2800
#define ENV_SED_SECTOR    0x14C08
#define ENV_SED_SIZE      0x1000

void main_sound_open_generic_sfx(void) {
    s32 i;

    SuzukiSPUInitialiser(0);
    main_heap_clear_smd_allocator_table();
    main_sound_load_waveset(
        main_file_load_to_address(WAVESET_WD_SECTOR, WAVESET_WD_SIZE, (void*)g_main_heap_low_overlay_load_address));
    SuzukiAppendVFXSMD(main_file_load_to_address(SYSTEM_SED_SECTOR, SYSTEM_SED_SIZE, g_main_sound_system_sed_data));
    SuzukiAppendVFXSMD(main_file_load_to_address(ENV_SED_SECTOR, ENV_SED_SIZE, g_main_sound_env_sed_data));

    g_main_sound_music.state.forced_music = 0;
    g_main_sound_music.slots.tune = 0;
    g_main_sound_music.slots.handles[0] = 0;
    for (i = 6; i >= 0; i--) {
        g_main_sound_music.slots.handles[i + 1] = 0;
    }

    g_main_sound_weather_sfx_id = 0;
    g_main_sound_music.slots.smd[3]
        = main_file_get_smd(g_main_sound_permanent_smd_files[0].sector, g_main_sound_permanent_smd_files[0].size);
    g_main_sound_music.slots.smd[4]
        = main_file_get_smd(g_main_sound_permanent_smd_files[1].sector, g_main_sound_permanent_smd_files[1].size);
    g_main_sound_music.slots.smd[5]
        = main_file_get_smd(g_main_sound_permanent_smd_files[2].sector, g_main_sound_permanent_smd_files[2].size);
    g_main_sound_music.slots.smd[6]
        = main_file_get_smd(g_main_sound_permanent_smd_files[3].sector, g_main_sound_permanent_smd_files[3].size);
    g_main_sound_music.slots.smd[7]
        = main_file_get_smd(g_main_sound_permanent_smd_files[4].sector, g_main_sound_permanent_smd_files[4].size);
}
