#include "fft/main.h"
#include "psx/types.h"

/* Byte budgets of the resident sound files. */
#define WAVESET_WD_SIZE 0x79000
#define SYSTEM_SED_SIZE 0x2800
#define ENV_SED_SIZE    0x1000

/* Start the sound driver and load the resident sound data: WAVESET.WD is read to
 * the low overlay address and uploaded to SPU RAM, SYSTEM.SED and ENV.SED are
 * appended to the driver's sound resources, the music slots are cleared, and the
 * five files of g_main_sound_permanent_smd_files are read into SMD slots 3..7. */
void main_sound_open_generic_sfx(void) {
    s32 i;

    suzuki_sound_init(0);
    main_heap_clear_smd_allocator_table();
    suzuki_sound_load_waveset(
        main_file_load_to_address(DISC_SECTOR_SOUND_WAVESET_WD, WAVESET_WD_SIZE, g_main_heap_low_overlay_load_address));
    suzuki_sound_append_resource(
        main_file_load_to_address(DISC_SECTOR_SOUND_SYSTEM_SED, SYSTEM_SED_SIZE, g_main_sound_system_sed_data));
    suzuki_sound_append_resource(
        main_file_load_to_address(DISC_SECTOR_SOUND_ENV_SED, ENV_SED_SIZE, g_main_sound_env_sed_data));

    g_main_sound_music.state.current_smd = 0;
    g_main_sound_music.slots.tune = 0;
    g_main_sound_music.slots.loading_slot = 0;
    for (i = 6; i >= 0; i--) {
        g_main_sound_music.slots.music[i] = 0;
    }

    g_main_sound_weather_sfx_id = 0;
    g_main_sound_music.slots.smd[2]
        = main_file_get_smd(g_main_sound_permanent_smd_files[0].sector, g_main_sound_permanent_smd_files[0].size);
    g_main_sound_music.slots.smd[3]
        = main_file_get_smd(g_main_sound_permanent_smd_files[1].sector, g_main_sound_permanent_smd_files[1].size);
    g_main_sound_music.slots.smd[4]
        = main_file_get_smd(g_main_sound_permanent_smd_files[2].sector, g_main_sound_permanent_smd_files[2].size);
    g_main_sound_music.slots.smd[5]
        = main_file_get_smd(g_main_sound_permanent_smd_files[3].sector, g_main_sound_permanent_smd_files[3].size);
    g_main_sound_music.slots.smd[6]
        = main_file_get_smd(g_main_sound_permanent_smd_files[4].sector, g_main_sound_permanent_smd_files[4].size);
}
