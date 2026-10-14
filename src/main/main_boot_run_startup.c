#include "fft/main.h"

/* Power-on initialization: install the VSync, DrawSync and CD callbacks, start
 * the GPU, pad, SPU and CD, show the two logos, open the memory-card events and
 * load the shared sounds and the zodiac frame while the Squaresoft logo is up.
 *
 * rand is seeded with 1, and g_main_boot_startup2_has_run is cleared so the first
 * main_boot_reset_game_state does not repeat the hardware restart. */
void main_boot_run_startup(void) {
    main_heap_clear_game_allocator_table();
    ResetCallback();

    g_main_system_play_time_hours = 0;
    g_main_system_play_time_minutes = 0;
    g_main_system_play_time_seconds = 0;
    g_main_system_play_time_frames = 0;

    VSyncCallback(main_system_handle_vsync_callback);
    DrawSyncCallback(main_system_handle_draw_sync_callback);
    CdReadyCallback(main_file_handle_cd_ready_callback);
    CdReadCallback(main_file_handle_cd_read_callback);

    ResetGraph(0);
    SetGraphDebug(0);
    PadInit(0);
    SpuInit();
    main_file_reset_cdrom_cpu_ram();
    main_gfx_reset_display(256, 240, 512, 0, 0, 0);
    main_boot_build_and_draw_sceap_logo();
    main_boot_build_and_draw_squaresoft_logo();
    main_card_init_events();
    main_sound_open_generic_sfx();
    main_gfx_load_zodiac_frame();
    srand(1);
    main_boot_fade_out_squaresoft_logo();
    g_main_boot_startup2_has_run = 0;
}
