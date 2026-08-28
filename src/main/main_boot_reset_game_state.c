#include "fft/main_boot.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void main_boot_reset_game_state(void) {
    int frame;

    if (g_main_boot_startup2_has_run != 0) {
        main_heap_clear_game_allocator_table();
        main_sound_unload_scenario_music_and_tunes();
        g_main_system_play_time_hours = 0;
        g_main_system_play_time_minutes = 0;
        g_main_system_play_time_seconds = 0;
        g_main_system_play_time_frames = 0;
        ResetGraph(1);
        SetGraphDebug(0);
        SpuInitHot();
        main_file_reset_cdrom_cpu_ram();
        main_gfx_reset_display(256, 240, 512, 0, 0, 0);
        main_boot_build_and_draw_squaresoft_logo();
        main_sound_open_generic_sfx();
        main_gfx_load_zodiac_frame();
    }

    main_save_init_state_and_options();
    main_status_init_check_data();
    main_save_init_data_tables();
    main_item_init_order_tables();

    if (g_main_boot_startup2_has_run != 0) {
        for (frame = 0; frame < 60; frame++) {
            VSync(0);
        }
        main_boot_fade_out_squaresoft_logo();
    }

    g_main_boot_startup2_has_run = 1;
}
