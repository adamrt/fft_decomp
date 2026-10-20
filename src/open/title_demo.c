/* Idle title-menu movies return to the logo sequence when playback finishes
 * or the player presses Start. */

#include "fft/controller_stack.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/movie.h"
#include "fft/system.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Start one of the title screen's demo movies (FFTPRE, FFTPR or FFTUNIT.STR) at cd_mix_level with
 * no separate video end frame, and push handler 11 with the stream's start sector and length. */
void movie_title_demo_start(s32 start_sector, s32 stream_length, s32 end_frame, s32 cd_mix_level) {
    s32 controller;

    movie_stream_start(start_sector, end_frame, -1, cd_mix_level);
    g_system_runtime_flags &= ~SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
    controller = g_controller_depth;
    g_controller_records[controller].movie.start_sector = start_sector;
    g_controller_records[controller].movie.stream_length = stream_length;
    g_controller_handler_indices[controller] = CONTROLLER_TITLE_DEMO_MOVIE;
    g_controller_depth = controller + 1;
}

/* Handler 11, a title demo movie: Start stops the stream. Once the stream has
 * stopped, by itself or on the update after Start, the display is blanked and the
 * controller is replaced by the FFTST.STR logo movie, which leads back to the
 * title menu. */
void movie_update_title_demo(controller_record_t* unused_record) {
    if ((g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) == 0) {
        SetDispMask(0);
        g_gfx_active_buffer_index = 1;
        g_system_runtime_flags |= SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        g_controller_depth--;
        movie_logo_start(DISC_SECTOR_OPEN_FFTST_STR, 0x278a, 0x3df, 0x2de);
    } else if ((g_input_new_button_presses & PSX_PAD_START) != 0) {
        movie_stream_stop(1);
    }
}
