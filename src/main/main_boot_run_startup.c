#include "fft/main.h"
#include "psx/types.h"

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
    SsUtReverbOff();
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
