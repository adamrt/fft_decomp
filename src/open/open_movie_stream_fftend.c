#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Start the ending movie at CD mix level 0x7e and push handler 9. The second
 * argument, the stream's length in sectors, is not read. */
void open_movie_stream_fftend(s32 start_sector, s32 unused, s32 end_frame) {
    s32 controller;

    (void)unused;
    open_movie_start_stream(start_sector, end_frame, -1, 0x7e);
    g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
    controller = g_open_current_controller_index;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_FFTEND_MOVIE;
    g_open_current_controller_index = controller + 1;
}
