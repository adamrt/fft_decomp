#include "fft/open.h"

void open_movie_play_end(void) {
    g_open_current_controller_index = 0;
    g_active_graphics_buffer_index = 0;
    open_movie_stream_fftend(0x21908, 0x3858, 0x571);
}
