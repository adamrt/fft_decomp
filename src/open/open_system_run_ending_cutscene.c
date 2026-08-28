#include "fft/main_runtime.h"
#include "fft/open.h"
#include "psx/types.h"

/* Each of the two otag banks holds 16 words per graphics buffer. */
#define OPEN_OTAG_BUFFER_BYTES 64

void open_system_run_ending_cutscene(void) {
    u32* otag_0;
    u32* otag_1;

    open_movie_init_and_play_end();
    if ((g_open_system_runtime_flags & 1) != 0) {
        otag_0 = g_open_gfx_otags[0];
        otag_1 = g_open_gfx_otags[1];
        do {
            s32 graphics_buffer_index;
            u32* current_otag;

            main_file_poll_load(&g_open_file_header);
            open_gfx_clear_and_draw_current_frame();
            open_movie_update_stream_frame();
            graphics_buffer_index = g_active_graphics_buffer_index;
            /* index * 64 + bank: the retail addu sums the offset first. */
            current_otag = (u32*)(graphics_buffer_index * OPEN_OTAG_BUFFER_BYTES + (u32)otag_0);
            open_script_draw_text_records(current_otag, graphics_buffer_index);
            open_controller_dispatch_current();
            DrawSync(0);
            open_input_update_buttons_and_check_game_reset();
            VSync(g_open_system_vsync_mode);
            open_movie_present_frame();
            current_otag = (u32*)((g_active_graphics_buffer_index ^ 1) * OPEN_OTAG_BUFFER_BYTES + (u32)otag_1);
            DrawOTag(current_otag - 1);
            main_noop_800449ec();
        } while ((g_open_system_runtime_flags & 1) != 0);
    }

    open_gfx_backup_frame_vram_page();
    SetDispMask(0);
    open_sound_set_type_and_volume(0xc0, 0x78);
}
