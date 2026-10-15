#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Each graphics buffer's ordering table holds 16 32-bit entries. */
#define OPEN_OTAG_BUFFER_BYTES 64

/* Entry point of OPEN.BIN for the ending: play FFTEND.STR, then run the ending
 * script until it finishes.
 *
 * Each frame polls the CD read, clears the ordering table, steps the movie,
 * queues the script's primitives, runs the current controller, reads the pad and
 * calls VSync with g_open_system_vsync_mode (0 during the movie, 2 while the
 * script runs). The ordering table is drawn after the buffer flip, on top of the
 * background open_movie_present_frame has just put into the new draw buffer. On
 * exit the VRAM page saved by open_movie_init_and_play_end is restored, the
 * display is blanked and the CD audio returns to mix level 0xc0. */
void open_system_run_ending_cutscene(void) {
    u32* otag_0;
    u32* otag_1;

    open_movie_init_and_play_end();
    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RUNNING) != 0) {
        otag_0 = g_open_gfx_otags[0];
        otag_1 = g_open_gfx_otags[1];
        do {
            s32 graphics_buffer_index;
            u32* current_otag;

            main_file_poll_load(&g_main_file_cd_state);
            open_gfx_clear_and_draw_current_frame();
            open_movie_update_stream_frame();
            graphics_buffer_index = g_active_graphics_buffer_index;
            /* index * 64 + base: the retail addu sums the offset first. */
            current_otag = (u32*)(graphics_buffer_index * OPEN_OTAG_BUFFER_BYTES + (u32)otag_0);
            open_script_draw_text_records(current_otag, graphics_buffer_index);
            open_controller_dispatch_current();
            DrawSync(0);
            open_input_update_buttons_and_check_game_reset();
            VSync(g_open_system_vsync_mode);
            open_movie_present_frame();
            current_otag = (u32*)((g_active_graphics_buffer_index ^ 1) * OPEN_OTAG_BUFFER_BYTES + (u32)otag_1);
            DrawOTag(current_otag - 1);
            main_system_frame_stub();
        } while ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RUNNING) != 0);
    }

    open_gfx_restore_frame_vram_page();
    SetDispMask(0);
    open_sound_set_cd_mix_and_volume(0xc0, 0x78);
}
