#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Start FFTEND.STR (sector 0x21908, ending at frame 0x571) as the first
 * controller. */
void open_movie_play_end(void) {
    g_open_current_controller_index = 0;
    g_active_graphics_buffer_index = 0;
    open_movie_stream_fftend(DISC_SECTOR_OPEN_FFTEND_STR, 0x3858, 0x571);
}
