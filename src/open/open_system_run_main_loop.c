#include "fft/open.h"
#include "psx/types.h"

/* Each graphics buffer's ordering table holds 16 32-bit entries. */
#define OPEN_OTAG_BUFFER_BYTES 64

/* Entry point of OPEN.BIN for the title sequence. Runs until a controller clears
 * OPEN_SYSTEM_RUNTIME_FLAG_RUNNING and returns g_open_system_result: 0 for a new
 * game, or a loaded save with a next scenario and no formation return event,
 * which main_system_run_game_loop takes straight into BATTLE; 1 for any other
 * loaded save and 2 for the tutorial, which go to WORLD.
 *
 * skip_movie starts at the title menu instead of the FFTST.STR logo movie. Each
 * frame polls the CD read, clears the ordering table and queues the overlay fade,
 * steps the movie, runs the WORLD menu frame while it is on and then the current
 * controller, draws the render records, reads the pad and flips the buffers. */
s32 open_system_run_main_loop(s32 skip_movie) {
    u32* otag_0;
    u32* otag_1;

    open_movie_init_and_stream_opening(skip_movie);
    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RUNNING) != 0) {
        otag_0 = g_open_gfx_otags[0];
        otag_1 = g_open_gfx_otags[1];
        do {
            s32 graphics_buffer_index;
            u32* current_otag;

            main_file_poll_load(&g_main_file_cd_state);
            open_gfx_clear_and_draw_current_frame();
            open_movie_update_stream_frame();
            open_menu_update_world_formation();
            open_controller_dispatch_current();

            graphics_buffer_index = g_active_graphics_buffer_index;
            /* index * 64 + base: the retail addu sums the offset first. */
            current_otag = (u32*)(graphics_buffer_index * OPEN_OTAG_BUFFER_BYTES + (u32)otag_0);
            window_dispatch_record_list_variant(
                current_otag, g_open_gfx_render_record_pointers, g_open_gfx_render_record_pointer_count);

            graphics_buffer_index = g_active_graphics_buffer_index;
            current_otag = (u32*)(graphics_buffer_index * OPEN_OTAG_BUFFER_BYTES + (u32)otag_1);
            DrawOTag(current_otag - 1);
            DrawSync(0);
            open_input_update_buttons_and_check_game_reset();
            VSync(0);
            open_movie_present_frame();
            main_system_frame_stub();
        } while ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RUNNING) != 0);
    }

    SetDispMask(0);
    open_sound_set_cd_mix_and_volume(0xc0, 0x78);
    return g_open_system_result;
}
